-module(zstd_tests).

-include_lib("eunit/include/eunit.hrl").

zstd_test() ->
    Data = <<"Hello, World!">>,
    ?assertEqual(Data,
                 zstd:decompress(
                     zstd:compress(Data))).

zstd_stream_test() ->
    Bin = << <<"A">> || _ <- lists:seq(1, 1024 * 1024) >>,
    CStream = zstd:new_compression_stream(),
    ok = zstd:compression_stream_init(CStream),
    {ok, CompressionBin} = zstd:stream_compress(CStream, Bin),
    {ok, FlushBin} = zstd:stream_flush(CStream),

    DStream = zstd:new_decompression_stream(),
    ok = zstd:decompression_stream_init(DStream),
    {ok, DBin1} = zstd:stream_decompress(DStream, CompressionBin),
    {ok, DBin2} = zstd:stream_decompress(DStream, FlushBin),
    ?assertEqual(Bin, <<DBin1/binary, DBin2/binary>>).
