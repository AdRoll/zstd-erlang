-module(zstd_tests).

-include_lib("eunit/include/eunit.hrl").

zstd_test() ->
    Data = <<"Hello, World!\n">>,
    ?assertEqual(Data,
                 zstd:decompress(
                     zstd:compress(Data))).

compress_to_file_test() ->
    Path = "/tmp/zstd_test.zst",
    Data = <<"Hello there!">>,
    compress_to_file_and_check(Path, Data).

compress_to_file_using_dirty_scheduler_test() ->
    Path = "/tmp/zstd_dirty_scheduler_test.zst",
    Data =
        base64:encode(
            crypto:strong_rand_bytes(64000)),
    compress_to_file_and_check(Path, Data).

compress_to_file_and_check(Path, Data) ->
    case filelib:is_regular(Path) of
        true ->
            file:delete(Path);
        _ ->
            ok
    end,
    ?assertEqual(ok, zstd:compress_to_file(Data, erlang:list_to_binary(Path))),
    {ok, ToDecompress} = file:read_file(Path),
    Decompressed = zstd:decompress(ToDecompress),
    ?assertEqual(Decompressed, Data).
