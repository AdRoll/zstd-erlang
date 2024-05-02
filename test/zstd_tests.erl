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

generate_randomkeys(Count, BucketRangeLow, BucketRangeHigh) ->
    generate_randomkeys(Count, [], BucketRangeLow, BucketRangeHigh).

generate_randomkeys(0, Acc, _BucketLow, _BucketHigh) ->
    Acc;
generate_randomkeys(Count, Acc, BucketLow, BRange) ->
    BNumber =
        lists:flatten(
            io_lib:format("~4..0B", [BucketLow + rand:uniform(BRange)])),
    KNumber =
        lists:flatten(
            io_lib:format("~4..0B", [rand:uniform(1000)])),
    K = {o, "Bucket" ++ BNumber, "Key" ++ KNumber, null},
    RandKey = {K, {Count + 1, {active, infinity}, erlang:phash2(K), null}},
    generate_randomkeys(Count - 1, [RandKey | Acc], BucketLow, BRange).

compression_perf_test_() ->
    {timeout, 60, fun compression_perf_testsizes/0}.

compression_perf_testsizes() ->
    compression_perf_tester(128),
    compression_perf_tester(256),
    compression_perf_tester(512),
    compression_perf_tester(1024),
    compression_perf_tester(2048),
    compression_perf_tester(4096),
    compression_perf_tester(8192).

compression_perf_tester(N) ->
    Loops = 100,
    {TotalCS, TotalDS, TotalDC, TotalDD, TotalQC, TotalQD, TotalAC, TotalAD} =
        lists:foldl(fun(_A, {CST, DST, CTDT, DTDT, CTQT, DTQT, CTT, DTT}) ->
                       RB0 = term_to_binary({base64:encode(
                                                 crypto:strong_rand_bytes(N * 8)),
                                             generate_randomkeys(N, 1, 4)}),
                       {CTD0, CD0} = timer:tc(fun() -> zstd:dirty_compress(RB0, 1) end),
                       {DTD0, DD0} = timer:tc(fun() -> zstd:dirty_decompress(CD0) end),
                       {CTQ0, CQ0} = timer:tc(fun() -> zstd:quick_compress(RB0, 1) end),
                       {DTQ0, DQ0} = timer:tc(fun() -> zstd:quick_decompress(CQ0) end),
                       {CT0, C0} = timer:tc(fun() -> zstd:compress(RB0) end),
                       {DT0, D0} = timer:tc(fun() -> zstd:decompress(C0) end),

                       ?assertMatch(RB0, DD0),
                       ?assertMatch(DD0, DQ0),
                       ?assertMatch(DQ0, D0),

                       {CST + byte_size(RB0),
                        DST + byte_size(C0),
                        CTDT + CTD0,
                        DTDT + DTD0,
                        CTQT + CTQ0,
                        DTQT + DTQ0,
                        CTT + CT0,
                        DTT + DT0}
                    end,
                    {0, 0, 0, 0, 0, 0, 0, 0},
                    lists:seq(1, Loops)),

    io:format(user,
              "Over ~w loops tested size ~w compress_size ~w~n"
              "mean compress time dirty_nif ~w quick_nif ~w auto_nif ~w~n"
              "mean decompress time dirty_nif ~w quick_nif ~w auto_nif ~w~n~n",
              [Loops,
               TotalCS div Loops,
               TotalDS div Loops,
               TotalDC div Loops,
               TotalQC div Loops,
               TotalAC div Loops,
               TotalDD div Loops,
               TotalQD div Loops,
               TotalAD div Loops]).
