#include "erl_nif.h"

#include <stdlib.h>
#include <zstd.h>

static const char* MODULE_NAME = "zstd";
static const char* COMPRESSION_STREAM_NAME = "CStream";
static const char* DECOMPRESSION_STREAM_NAME = "DStream";
static char* COMPRESS_CONTEXT_KEY = "zstd_compress_context_key";
static char* DECOMPRESS_CONTEXT_KEY = "zstd_decompress_context_key";

ErlNifTSDKey zstdDecompressContextKey;
ErlNifTSDKey zstdCompressContextKey;

static ErlNifResourceType *zstd_compression_stream_type = NULL;
static ErlNifResourceType *zstd_decompression_stream_type = NULL;

static ERL_NIF_TERM zstd_atom_ok;
static ERL_NIF_TERM zstd_atom_error;
static ERL_NIF_TERM zstd_atom_invalid;
static ERL_NIF_TERM zstd_atom_enomem;
static ERL_NIF_TERM zstd_atom_eof;
static ERL_NIF_TERM zstd_atom_compression;
static ERL_NIF_TERM zstd_atom_decompression;

static ERL_NIF_TERM zstd_nif_compress(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  ErlNifBinary bin, ret_bin;
  size_t buff_size, compressed_size;
  unsigned int compression_level;

  ZSTD_CCtx* ctx = (ZSTD_CCtx*)enif_tsd_get(zstdCompressContextKey);
  if (!ctx) {
      ctx = ZSTD_createCCtx();
      enif_tsd_set(zstdCompressContextKey, ctx);
  }

  if(!enif_inspect_binary(env, argv[0], &bin)
     || !enif_get_uint(env, argv[1], &compression_level)
     || compression_level > ZSTD_maxCLevel())
    return enif_make_badarg(env);

  buff_size = ZSTD_compressBound(bin.size);

  if(!enif_alloc_binary(buff_size, &ret_bin))
    return zstd_atom_error;

  compressed_size = ZSTD_compressCCtx(ctx, ret_bin.data, buff_size, bin.data, bin.size, compression_level);
  if(ZSTD_isError(compressed_size))
    return zstd_atom_error;

  if(!enif_realloc_binary(&ret_bin, compressed_size))
    return zstd_atom_error;

  return enif_make_binary(env, &ret_bin);
}

static ERL_NIF_TERM zstd_nif_decompress(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  ERL_NIF_TERM out;
  unsigned char *outp;
  ErlNifBinary bin;
  unsigned long long uncompressed_size;

  ZSTD_DCtx* ctx = (ZSTD_DCtx*)enif_tsd_get(zstdDecompressContextKey);
  if (!ctx) {
      ctx = ZSTD_createDCtx();
      enif_tsd_set(zstdDecompressContextKey, ctx);
  }

  if(!enif_inspect_binary(env, argv[0], &bin))
    return enif_make_badarg(env);

  uncompressed_size = ZSTD_getFrameContentSize(bin.data, bin.size);

  outp = enif_make_new_binary(env, uncompressed_size, &out);

  if(ZSTD_decompressDCtx(ctx, outp, uncompressed_size, bin.data, bin.size) != uncompressed_size)
    return zstd_atom_error;

  return out;
}

static ERL_NIF_TERM zstd_nif_new_compression_stream(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  /* create handle */
  ZSTD_CStream **handle = enif_alloc_resource(
      zstd_compression_stream_type,
      sizeof(ZSTD_CStream *)
  );

  /* create cstream stream */
  *handle = ZSTD_createCStream();
  
  ERL_NIF_TERM res = enif_make_resource(env, handle);
  enif_release_resource(handle);
  return res;
}

static ERL_NIF_TERM zstd_nif_new_decompression_stream(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  /* create handle */
  ZSTD_DStream **handle = enif_alloc_resource(
      zstd_decompression_stream_type,
      sizeof(ZSTD_DStream *)
  );

  /* create dstream stream */
  *handle = ZSTD_createDStream();
  
  ERL_NIF_TERM res = enif_make_resource(env, handle);
  enif_release_resource(handle);
  return res;
}

static ERL_NIF_TERM zstd_nif_init_compression_stream(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  int level = ZSTD_CLEVEL_DEFAULT;
  size_t ret;
  ZSTD_CStream **pzcs;

  /* extract the stream */
  if (!(enif_get_resource(env, argv[0], zstd_compression_stream_type, (void **)&pzcs)))
    return enif_make_tuple2(env, zstd_atom_error, zstd_atom_invalid);

  /* extract the compression level if any */
  if ((argc == 2) && !(enif_get_int(env, argv[1], &level)))
      return enif_make_badarg(env);

  /* initialize the stream */
  if (ZSTD_isError(ret = ZSTD_initCStream(*pzcs, level)))
      return enif_make_tuple2(env, zstd_atom_error, enif_make_string(env, ZSTD_getErrorName(ret), ERL_NIF_LATIN1));

  /* stream initialization successful */
  return zstd_atom_ok;
}

static ERL_NIF_TERM zstd_nif_init_decompression_stream(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  size_t ret;
  ZSTD_DStream **pzcs;

  /* extract the stream */
  if (!(enif_get_resource(env, argv[0], zstd_decompression_stream_type, (void **)&pzcs)))
    return enif_make_tuple2(env, zstd_atom_error, zstd_atom_invalid);

  /* initialize the stream */
  if (ZSTD_isError(ret = ZSTD_initDStream(*pzcs)))
      return enif_make_tuple2(env, zstd_atom_error, enif_make_string(env, ZSTD_getErrorName(ret), ERL_NIF_LATIN1));

  /* stream initialization successful */
  return zstd_atom_ok;
}

static ERL_NIF_TERM zstd_nif_reset_compression_stream(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  size_t ret;
  size_t size = ZSTD_CONTENTSIZE_UNKNOWN;
  ZSTD_CStream **pzcs;

  /* extract the stream */
  if (!(enif_get_resource(env, argv[0], zstd_compression_stream_type, (void **)&pzcs)))
      return enif_make_tuple2(env, zstd_atom_error, zstd_atom_invalid);

  /* extract the pledged source size if any */
  if ((argc == 2) && !(enif_get_ulong(env, argv[1], &size)))
      return enif_make_badarg(env);

  /* reset the stream */
  if (ZSTD_isError(ret = ZSTD_CCtx_reset(*pzcs, size)))
      return enif_make_tuple2(env, zstd_atom_error, enif_make_string(env, ZSTD_getErrorName(ret), ERL_NIF_LATIN1));

  /* stream resetting successful */
  return zstd_atom_ok;
}

static ERL_NIF_TERM zstd_nif_reset_decompression_stream(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  size_t ret;
  ZSTD_DStream **pzcs;

  /* extract the stream */
  if (!(enif_get_resource(env, argv[0], zstd_decompression_stream_type, (void **)&pzcs)))
      return enif_make_tuple2(env, zstd_atom_error, zstd_atom_invalid);

  /* reset the stream */
  if (ZSTD_isError(ret = ZSTD_DCtx_reset(*pzcs, ZSTD_reset_session_only)))
      return enif_make_tuple2(env, zstd_atom_error, enif_make_string(env, ZSTD_getErrorName(ret), ERL_NIF_LATIN1));

  /* stream resetting successful */
  return zstd_atom_ok;
}

static ERL_NIF_TERM zstd_nif_flush_compression_stream(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  size_t ret;
  ErlNifBinary bin;
  ZSTD_CStream **pzcs;

  /* extract the stream */
  if (!(enif_get_resource(env, argv[0], zstd_compression_stream_type, (void **)&pzcs)))
      return enif_make_tuple2(env, zstd_atom_error, zstd_atom_invalid);

  /* allocate binary buffer */
  if (!(enif_alloc_binary(ZSTD_CStreamOutSize(), &bin)))
      return enif_make_tuple2(env, zstd_atom_error, zstd_atom_enomem);

  /* output buffer */
  ZSTD_outBuffer outbuf = {
      .pos = 0,
      .dst = bin.data,
      .size = bin.size,
  };

  /* reset the stream */
  if (ZSTD_isError(ret = ZSTD_endStream(*pzcs, &outbuf)))
  {
      enif_release_binary(&bin);
      return enif_make_tuple2(env, zstd_atom_error, enif_make_string(env, ZSTD_getErrorName(ret), ERL_NIF_LATIN1));
  }

  /* transfer to binary object */
  ERL_NIF_TERM binary = enif_make_binary(env, &bin);
  ERL_NIF_TERM result = binary;

  /* remove unused spaces */
  if (outbuf.pos < outbuf.size)
      result = enif_make_sub_binary(env, binary, 0, outbuf.pos);

  /* construct the result tuple */
  return enif_make_tuple2(env, zstd_atom_ok, result);
}

static ERL_NIF_TERM zstd_nif_compress_stream(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    size_t ret;
    ErlNifBinary in;
    ErlNifBinary out;
    ZSTD_CStream **pzcs;

    /* extract the stream */
    if (!(enif_get_resource(env, argv[0], zstd_compression_stream_type, (void **)&pzcs)) ||
        !(enif_inspect_iolist_as_binary(env, argv[1], &in)))
        return enif_make_tuple2(env, zstd_atom_error, zstd_atom_invalid);

    /* all output binary buffer */
    if (!(enif_alloc_binary(ZSTD_compressBound(in.size), &out))) {
        enif_release_binary(&in);
        return enif_make_tuple2(env, zstd_atom_error, zstd_atom_enomem);
    }

    /* input buffer */
    ZSTD_inBuffer inbuf = {
        .pos = 0,
        .src = in.data,
        .size = in.size,
    };

    /* output buffer */
    ZSTD_outBuffer outbuf = {
        .pos = 0,
        .dst = out.data,
        .size = out.size,
    };

    /* compress every chunk */
    while (inbuf.pos < inbuf.size) {
        if (ZSTD_isError(ret = ZSTD_compressStream(*pzcs, &outbuf, &inbuf))) {
            enif_release_binary(&in);
            enif_release_binary(&out);
            return enif_make_tuple2(env, zstd_atom_error, enif_make_string(env, ZSTD_getErrorName(ret), ERL_NIF_LATIN1));
        }
    }

    /* transfer to binary object */
    ERL_NIF_TERM binary = enif_make_binary(env, &out);
    ERL_NIF_TERM result = binary;

    /* remove unused spaces */
    if (outbuf.pos < outbuf.size)
        result = enif_make_sub_binary(env, binary, 0, outbuf.pos);

    /* construct the result tuple */
    enif_release_binary(&in);
    return enif_make_tuple2(env, zstd_atom_ok, result);
}

static ERL_NIF_TERM zstd_nif_decompress_stream(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  size_t ret;
  ErlNifBinary in;
  ErlNifBinary out;
  ZSTD_DStream **pzds;

  /* extract the stream */
  if (!(enif_get_resource(env, argv[0], zstd_decompression_stream_type, (void **)&pzds)) ||
      !(enif_inspect_iolist_as_binary(env, argv[1], &in)))
      return enif_make_tuple2(env, zstd_atom_error, zstd_atom_invalid);

  /* allocate output binary buffer */
  if (!(enif_alloc_binary(ZSTD_DStreamOutSize(), &out))) {
    enif_release_binary(&in);
    return enif_make_tuple2(env, zstd_atom_error, zstd_atom_enomem);
  }

  /* input buffer */
  ZSTD_inBuffer inbuf = {
      .pos = 0,
      .src = in.data,
      .size = in.size,
  };

  /* output buffer */
  ZSTD_outBuffer outbuf = {
      .pos = 0,
      .dst = out.data,
      .size = out.size,
  };

  /* decompress every chunk */
  while (inbuf.pos < inbuf.size) {
    /* enlarge output buffer as needed */
    if ((outbuf.size - outbuf.pos) < ZSTD_DStreamOutSize()) {
      /* resize the output binary */
      if (!(enif_realloc_binary(&out, out.size + ZSTD_DStreamOutSize()))) {
        enif_release_binary(&in);
        enif_release_binary(&out);
        return enif_make_tuple2(env, zstd_atom_error, zstd_atom_enomem);
      }

      /* update buffer pointers */
      outbuf.dst = out.data;
      outbuf.size = out.size;
    }

    /* decompress one frame */
    if (ZSTD_isError(ret = ZSTD_decompressStream(*pzds, &outbuf, &inbuf))) {
      enif_release_binary(&in);
      enif_release_binary(&out);
      return enif_make_tuple2(env, zstd_atom_error, enif_make_string(env, ZSTD_getErrorName(ret), ERL_NIF_LATIN1));
    }
  }

  /* transfer to binary object */
  ERL_NIF_TERM binary = enif_make_binary(env, &out);
  ERL_NIF_TERM result = binary;

  /* remove unused spaces */
  if (outbuf.pos < outbuf.size)
    result = enif_make_sub_binary(env, binary, 0, outbuf.pos);

  /* construct the result tuple */
  enif_release_binary(&in);
  return enif_make_tuple2(env, zstd_atom_ok, result);
}


static void zstd_compression_stream_destructor(ErlNifEnv *env, void *stream) {
    ZSTD_CStream **handle = stream;
    ZSTD_freeCStream(*handle);
}

static void zstd_decompression_stream_destructor(ErlNifEnv *env, void *stream) {
    ZSTD_DStream **handle = stream;
    ZSTD_freeDStream(*handle);
}

static int zstd_init(ErlNifEnv *env) {
  // For compress and decompress
  enif_tsd_key_create(COMPRESS_CONTEXT_KEY, &zstdCompressContextKey);
  enif_tsd_key_create(DECOMPRESS_CONTEXT_KEY, &zstdDecompressContextKey);

  // Compression stream type
  zstd_compression_stream_type = enif_open_resource_type(
      env,
      MODULE_NAME,
      COMPRESSION_STREAM_NAME,
      zstd_compression_stream_destructor,
      ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER,
      NULL
  );

  // Decompression stream type
  zstd_decompression_stream_type = enif_open_resource_type(
      env,
      MODULE_NAME,
      DECOMPRESSION_STREAM_NAME,
      zstd_decompression_stream_destructor,
      ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER,
      NULL
  );

  // Create atoms
  enif_make_existing_atom(env, "ok" , &zstd_atom_ok , ERL_NIF_LATIN1);
  enif_make_existing_atom(env, "error" , &zstd_atom_error , ERL_NIF_LATIN1);
  enif_make_existing_atom(env, "invalid" , &zstd_atom_invalid , ERL_NIF_LATIN1);
  enif_make_existing_atom(env, "enomem", &zstd_atom_enomem, ERL_NIF_LATIN1);
  enif_make_existing_atom(env, "eof", &zstd_atom_eof, ERL_NIF_LATIN1);
  enif_make_existing_atom(env, "compression" , &zstd_atom_compression , ERL_NIF_LATIN1);
  enif_make_existing_atom(env, "decompression" , &zstd_atom_decompression , ERL_NIF_LATIN1);

  /* should all be loaded */
  return !(zstd_compression_stream_type && zstd_decompression_stream_type);
}

static int zstd_on_load(ErlNifEnv *env, void **priv, ERL_NIF_TERM info) {
  return zstd_init(env);
}

static int zstd_on_reload(ErlNifEnv *env, void **priv, ERL_NIF_TERM info) {
  return zstd_init(env);
}

static int zstd_on_upgrade(ErlNifEnv *env, void **priv, void **old, ERL_NIF_TERM info) {
  return zstd_init(env);
}

static ErlNifFunc nif_funcs[] = {
  { "compress"                    , 2, zstd_nif_compress                   , ERL_DIRTY_JOB_CPU_BOUND },
  { "decompress"                  , 1, zstd_nif_decompress                 , ERL_DIRTY_JOB_CPU_BOUND },

  { "new_compression_stream"      , 0, zstd_nif_new_compression_stream                               },
  { "new_decompression_stream"    , 0, zstd_nif_new_decompression_stream                             },

  { "compression_stream_init"     , 1, zstd_nif_init_compression_stream    , ERL_DIRTY_JOB_CPU_BOUND },
  { "compression_stream_init"     , 2, zstd_nif_init_compression_stream    , ERL_DIRTY_JOB_CPU_BOUND },
  { "decompression_stream_init"   , 1, zstd_nif_init_decompression_stream  , ERL_DIRTY_JOB_CPU_BOUND },

  { "compression_stream_reset"    , 2, zstd_nif_reset_compression_stream                             },
  { "compression_stream_reset"    , 1, zstd_nif_reset_compression_stream                             },
  { "decompression_stream_reset"  , 1, zstd_nif_reset_decompression_stream                           },

  { "stream_flush"                , 1, zstd_nif_flush_compression_stream   , ERL_DIRTY_JOB_CPU_BOUND },
  { "stream_compress"             , 2, zstd_nif_compress_stream            , ERL_DIRTY_JOB_CPU_BOUND },
  { "stream_decompress"           , 2, zstd_nif_decompress_stream          , ERL_DIRTY_JOB_CPU_BOUND }
};

ERL_NIF_INIT(zstd, nif_funcs, zstd_on_load, zstd_on_reload, zstd_on_upgrade, NULL);
