zstd-erlang
=====

[![Hex.pm](https://img.shields.io/hexpm/v/zstd.svg)](https://hex.pm/packages/zstd)

Zstd binding for Erlang/Elixir

http://facebook.github.io/zstd/

The current version uses [v1.5.4](https://github.com/facebook/zstd/releases/tag/v1.5.4)

Usage
-----

```
zstd:compress(Binary).
zstd:compress(Binary, CompressionLevel).
zstd:decompress(Binary).
```

```
1> Compressed = zstd:compress(<<"Hello, World!">>).
<<40,181,47,253,32,13,105,0,0,72,101,108,108,111,44,32,87,
  111,114,108,100,33>>
2> zstd:decompress(Compressed).
<<"Hello, World!">>
```

```
compress_file() ->
    {ok, File} = file:open("test.txt.zst", [write, raw, delayed_write, sync]),
    Bin = << <<"A">> || _ <- lists:seq(1, 1024 * 1024) >>,
    CStream = zstd:new_compression_stream(),
    ok = zstd:compression_stream_init(CStream),
    {ok, ResC} = zstd:stream_compress(CStream, Bin),
    file:write(File, ResC),
    {ok, ResF} = zstd:stream_flush(CStream),
    file:write(File, ResF),
    file:close(File).

decompress_file() ->
    {ok, Data} = file:read_file("test.txt.zst"),
    DStream = zstd:new_decompression_stream(),
    ok = zstd:decompression_stream_init(DStream),
    {ok, Bin} = zstd:stream_decompress(DStream, Data),
    io:format("~s", [Bin]),
    io:format("Size: ~p~n", [size(Bin)]).

decompress_large_file() ->
    DStream = zstd:new_decompression_stream(),
    ok = zstd:decompression_stream_init(DStream),

    {ok, RFile} = file:open("somefile.log.ztd", [read, raw, binary, read_ahead]),
    {ok, WFile} = file:open("somefile.log", [write, raw, delayed_write, sync]),
    read_file(RFile, WFile, DStream),
    file:close(RFile),
    file:close(WFile).

read_file(RFile, WFile, DStream) ->
    case file:read(RFile, 1024 * 8) of
        {ok, Data} ->
            {ok, Bin} = zstd:stream_decompress(DStream, Data),
            file:write(WFile, Bin),
            read_file(RFile, WFile, DStream);
        eof ->
            ok
    end.
```

#### For Elixir

```
iex(1)> compressed = :zstd.compress("Hello, World!")
<<40, 181, 47, 253, 32, 13, 105, 0, 0, 72, 101, 108, 108, 111, 44, 32, 87, 111,
  114, 108, 100, 33>>
iex(2)> :zstd.decompress(compressed)
"Hello, World!"
```
