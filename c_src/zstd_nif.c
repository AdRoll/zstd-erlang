#include "erl_nif.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <zstd.h>

#define MAX_BYTES_TO_NIF 20000

ErlNifTSDKey zstdDecompressContextKey;
ErlNifTSDKey zstdCompressContextKey;
ErlNifTSDKey zstdCompressToFileContextKey;

static ERL_NIF_TERM do_compress_to_file(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);


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
    return enif_make_atom(env, "error");

  compressed_size = ZSTD_compressCCtx(ctx, ret_bin.data, buff_size, bin.data, bin.size, compression_level);
  if(ZSTD_isError(compressed_size))
    return enif_make_atom(env, "error");

  if(!enif_realloc_binary(&ret_bin, compressed_size))
    return enif_make_atom(env, "error");

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

  uncompressed_size = ZSTD_getDecompressedSize(bin.data, bin.size);

  outp = enif_make_new_binary(env, uncompressed_size, &out);

  if(ZSTD_decompressDCtx(ctx, outp, uncompressed_size, bin.data, bin.size) != uncompressed_size)
    return enif_make_atom(env, "error");

  return out;
}

/*! fopen_orDie() :
 * Open a file using given file path and open option.
 *
 * @return If successful this function will return a FILE pointer to an
 * opened file otherwise it sends an error to stderr and exits.
 */
static FILE* fopen_orDie(const char *filename, const char *instruction)
{
    FILE* const inFile = fopen(filename, instruction);
    if (inFile) return inFile;
    /* error */
    exit(2);
}

/*! saveFile_orDie() :
 *
 * Save buffSize bytes to a given file path, obtaining them from a location pointed
 * to by buff.
 *
 * Note: This function will send an error to stderr and exit if it
 * cannot write to a given file.
 */
static void saveFile_orDie(const char* fileName, const void* buff, size_t buffSize)
{
    FILE* const oFile = fopen_orDie(fileName, "a");
    size_t const wSize = fwrite(buff, 1, buffSize, oFile);
    if (wSize != (size_t)buffSize) {
        exit(5);
    }
    if (fclose(oFile)) {
        exit(3);
    }
}
static ERL_NIF_TERM zstd_nif_compress_to_file(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    ErlNifBinary bin;
    if(!enif_inspect_binary(env, argv[0], &bin)) return enif_make_badarg(env);
    if (bin.size > MAX_BYTES_TO_NIF) {
        return enif_schedule_nif(env, "do_compress_to_file", ERL_NIF_DIRTY_JOB_CPU_BOUND, do_compress_to_file, argc, argv);
    }

    return do_compress_to_file(env, argc, argv);
}

static ERL_NIF_TERM do_compress_to_file(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  ErlNifBinary bin, ret_bin, arg_path;
  size_t buff_size, compressed_size;
  unsigned int compression_level;
  char* path;

  ZSTD_CCtx* ctx = (ZSTD_CCtx*)enif_tsd_get(zstdCompressToFileContextKey);
  if (!ctx) {
      ctx = ZSTD_createCCtx();
      enif_tsd_set(zstdCompressToFileContextKey, ctx);
  }

  if(!enif_inspect_binary(env, argv[0], &bin)
     || !enif_inspect_binary(env, argv[1], &arg_path)
     || !enif_get_uint(env, argv[2], &compression_level)
     || compression_level > ZSTD_maxCLevel())
    return enif_make_badarg(env);

  buff_size = ZSTD_compressBound(bin.size);

  if(!enif_alloc_binary(buff_size, &ret_bin))
    return enif_make_atom(env, "error");

  compressed_size = ZSTD_compressCCtx(ctx, ret_bin.data, buff_size, bin.data, bin.size, compression_level);
  if(ZSTD_isError(compressed_size))
    return enif_make_atom(env, "error");

  if(!enif_realloc_binary(&ret_bin, compressed_size))
    return enif_make_atom(env, "error");

  path = (char *)arg_path.data;
  
  saveFile_orDie(path, ret_bin.data, compressed_size);
  
  return enif_make_atom(env, "ok");
}

static ErlNifFunc nif_funcs[] = {
    {"compress", 2, zstd_nif_compress},
    {"decompress", 1, zstd_nif_decompress},
    {"compress_to_file", 3, zstd_nif_compress_to_file},
};

static int load(ErlNifEnv* env, void** priv_data, ERL_NIF_TERM load_info)
 {
 enif_tsd_key_create("zstd_decompress_context_key", &zstdDecompressContextKey);
 enif_tsd_key_create("zstd_compress_context_key", &zstdCompressContextKey);
 enif_tsd_key_create("zstd_compress_to_file_context_key", &zstdCompressToFileContextKey);
 return 0;
 }

ERL_NIF_INIT(zstd, nif_funcs, load, NULL, NULL, NULL);
