-module(zstd).

-export([compress/1, compress/2]).
-export([decompress/1]).
-export([quick_compress/2, quick_decompress/1]).
-export([dirty_compress/2, dirty_decompress/1]).
-export([new_compression_stream/0, new_decompression_stream/0, compression_stream_init/1,
         compression_stream_init/2, decompression_stream_init/1, compression_stream_reset/2,
         compression_stream_reset/1, decompression_stream_reset/1, stream_flush/1,
         stream_compress/2, stream_decompress/2]).

-on_load init/0.

-define(APPNAME, zstd).
-define(LIBNAME, zstd_nif).
% Thresholds at which it is preferable to use a dirty_nif
-define(UNCOMPRESSED_SIZE_DIRTY, 250000).
-define(COMPRESSED_SIZE_DIRTY, 50000).

-spec compress(Uncompressed :: binary()) -> Compressed :: binary().
compress(Binary) ->
    compress(Binary, 1).

-spec compress(Uncompressed :: binary(), CompressionLevel :: 0..22) ->
                  Compressed :: binary().
compress(Uncompressed, Level) when byte_size(Uncompressed) > ?UNCOMPRESSED_SIZE_DIRTY ->
    dirty_compress(Uncompressed, Level);
compress(Uncompressed, Level) ->
    quick_compress(Uncompressed, Level).

-spec dirty_compress(Uncompressed :: binary(), CompressionLevel :: 0..22) ->
                        Compressed :: binary().
dirty_compress(_, _) ->
    erlang:nif_error(?LINE).

-spec quick_compress(Uncompressed :: binary(), CompressionLevel :: 0..22) ->
                        Compressed :: binary().
quick_compress(_, _) ->
    erlang:nif_error(?LINE).

-spec decompress(Compressed :: binary()) -> Uncompressed :: binary() | error.
decompress(Compressed) when byte_size(Compressed) > ?COMPRESSED_SIZE_DIRTY ->
    dirty_decompress(Compressed);
decompress(Compressed) ->
    quick_decompress(Compressed).

-spec dirty_decompress(Compressed :: binary()) -> Uncompressed :: binary() | error.
dirty_decompress(_) ->
    erlang:nif_error(?LINE).

-spec quick_decompress(Compressed :: binary()) -> Uncompressed :: binary() | error.
quick_decompress(_) ->
    erlang:nif_error(?LINE).

-spec new_compression_stream() -> reference().
new_compression_stream() ->
    erlang:nif_error(?LINE).

-spec new_decompression_stream() -> reference().
new_decompression_stream() ->
    erlang:nif_error(?LINE).

-spec compression_stream_init(reference()) -> ok | {error, invalid | string()}.
compression_stream_init(_Ref) ->
    erlang:nif_error(?LINE).

-spec compression_stream_init(reference(), 0..22) -> ok | {error, invalid | string()}.
compression_stream_init(_Ref, _Level) ->
    erlang:nif_error(?LINE).

-spec decompression_stream_init(reference()) -> ok | {error, invalid | string()}.
decompression_stream_init(_Ref) ->
    erlang:nif_error(?LINE).

-spec compression_stream_reset(reference()) -> ok | {error, invalid | string()}.
compression_stream_reset(_Ref) ->
    erlang:nif_error(?LINE).

-spec compression_stream_reset(reference(), non_neg_integer()) ->
                                  ok | {error, invalid | string()}.
compression_stream_reset(_Ref, _Size) ->
    erlang:nif_error(?LINE).

-spec decompression_stream_reset(reference()) -> ok | {error, invalid | string()}.
decompression_stream_reset(_Ref) ->
    erlang:nif_error(?LINE).

-spec stream_flush(reference()) -> {ok, binary()} | {error, invalid | enomem | string()}.
stream_flush(_Ref) ->
    erlang:nif_error(?LINE).

-spec stream_compress(reference(), iodata()) ->
                         {ok, binary()} | {error, invalid | enomem | string()}.
stream_compress(_Ref, _IOData) ->
    erlang:nif_error(?LINE).

-spec stream_decompress(reference(), iodata()) ->
                           {ok, binary()} | {error, invalid | enomem | string()}.
stream_decompress(_Ref, _Binary) ->
    erlang:nif_error(?LINE).

init() ->
    SoName =
        case code:priv_dir(?APPNAME) of
            {error, bad_name} ->
                case filelib:is_dir(
                         filename:join(["..", priv]))
                of
                    true ->
                        filename:join(["..", priv, ?LIBNAME]);
                    _ ->
                        filename:join([priv, ?LIBNAME])
                end;
            Dir ->
                filename:join(Dir, ?LIBNAME)
        end,
    erlang:load_nif(SoName, 0).
