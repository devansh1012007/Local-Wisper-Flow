$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$cache = Join-Path $root "_deps-cache"
New-Item -ItemType Directory -Force -Path $cache | Out-Null

$grpcTag = "v1.71.0"
$llamaTag = "b10603"
$llamaUrl = $env:LLAMA_URL
if (-not $llamaUrl) {
    $llamaUrl = "https://github.com/ggml-org/llama.cpp/archive/refs/tags/$llamaTag.tar.gz"
}
$grpcUrl = $env:GRPC_URL
if (-not $grpcUrl) {
    $grpcUrl = "https://github.com/grpc/grpc/archive/refs/tags/$grpcTag.tar.gz"
}

$deps = @(
    @{ name = "abseil-cpp-4447c7562e3bc702ade25105912dce503f0c4010.tar.gz"; url = "https://github.com/abseil/abseil-cpp/archive/4447c7562e3bc702ade25105912dce503f0c4010.tar.gz"; target = "abseil-cpp" },
    @{ name = "boringssl-c64b8fe.tar.gz"; url = "https://github.com/google/boringssl/archive/c64b8fe.tar.gz"; target = "boringssl-with-bazel" },
    @{ name = "cares-6360e96.tar.gz"; url = "https://github.com/c-ares/c-ares/archive/6360e96.tar.gz"; target = "cares/cares" },
    @{ name = "protobuf-2d4414f384dc499af113b5991ce3eaa9df6dd931.tar.gz"; url = "https://github.com/protocolbuffers/protobuf/archive/2d4414f384dc499af113b5991ce3eaa9df6dd931.tar.gz"; target = "protobuf" },
    @{ name = "re2-0c5616df9c0aaa44c9440d87422012423d91c7d1.tar.gz"; url = "https://github.com/google/re2/archive/0c5616df9c0aaa44c9440d87422012423d91c7d1.tar.gz"; target = "re2" },
    @{ name = "zlib-09155eaa2f9270dc4ed1fa13e2b4b2613e6e4851.tar.gz"; url = "https://github.com/madler/zlib/archive/09155eaa2f9270dc4ed1fa13e2b4b2613e6e4851.tar.gz"; target = "zlib" }
)

function Fetch([string]$name, [string]$url) {
    $path = Join-Path $cache $name
    if (Test-Path $path) {
        Write-Host "have $name"
        return
    }
    Write-Host "downloading $name"
    Invoke-WebRequest -Uri $url -OutFile $path -MaximumRetryCount 3
}

Fetch "llama-$llamaTag.tar.gz" $llamaUrl
Fetch "grpc-$grpcTag.tar.gz" $grpcUrl
foreach ($dep in $deps) {
    Fetch $dep.name $dep.url
}

$fullTarball = Join-Path $cache "grpc-$grpcTag-full.tar.gz"
if (Test-Path $fullTarball) {
    Write-Host "have grpc-$grpcTag-full.tar.gz"
    exit 0
}

Write-Host "building grpc-$grpcTag-full.tar.gz"
$work = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString())
New-Item -ItemType Directory -Force -Path $work | Out-Null
try {
    tar -xzf (Join-Path $cache "grpc-$grpcTag.tar.gz") -C $work
    $grpcDir = Join-Path $work "grpc-$($grpcTag.TrimStart('v'))"
    foreach ($dep in $deps) {
        tar -xzf (Join-Path $cache $dep.name) -C $work
        $listing = tar -tzf (Join-Path $cache $dep.name)
        $srcDir = ($listing | Select-Object -First 1).Split("/")[0]
        $targetPath = Join-Path $grpcDir ("third_party/" + $dep.target)
        $targetParent = Split-Path -Parent $targetPath
        New-Item -ItemType Directory -Force -Path $targetParent | Out-Null
        if (Test-Path $targetPath) {
            Remove-Item -Recurse -Force $targetPath
        }
        Move-Item (Join-Path $work $srcDir) $targetPath
    }
    tar -czf $fullTarball -C $work (Split-Path -Leaf $grpcDir)
    Write-Host "wrote $fullTarball"
}
finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
