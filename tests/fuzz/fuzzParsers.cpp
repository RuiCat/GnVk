// tests/fuzz/fuzzParsers.cpp — B9-5 解析器模糊测试（oss-fuzz 风格本地变异循环）
//
// 目标（纯 CPU，无 GPU 依赖）：PNG/JPEG 解码、glTF/GLB 资产、PMD/PMX 模型、WAV 音频。
// 用法：FUZZ_SECS=3600 FUZZ_SEED=1 ./fuzzParsers [corpus_dir]
//   每次迭代把当前输入写到 $FUZZ_ARTIFACT（默认 .local/fuzz/last.bin），
//   ASan/UBSan 命中即中止进程，该文件即崩溃复现样本。
// 构建：cmake -S . -B build-fuzz -DCMAKE_BUILD_TYPE=RelWithDebInfo \
//        -DCMAKE_CXX_FLAGS="-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer" \
//        -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" \
//        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
//        cmake --build build-fuzz --target fuzzParsers
#include <GN.h>
#include "GN_3Dtexture.h"
#include "GN_3Dasset.h"
#include "pmdLoader.h"
#include "pmxLoader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace {

uint64_t g_rng = 0x9e3779b97f4a7c15ULL;
uint64_t rnd() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17;
    return g_rng;
}
uint32_t rndN(uint32_t n) { return n ? (uint32_t)(rnd() % n) : 0; }

std::string g_artifact = ".local/fuzz/last.bin";

void writeArtifact(const std::vector<uint8_t>& b) {
    FILE* f = fopen(g_artifact.c_str(), "wb");
    if(f) { fwrite(b.data(), 1, b.size(), f); fclose(f); }
}

// 6 个解析目标
void dispatch(const std::vector<uint8_t>& b, int target) {
    const uint8_t* d = b.data();
    size_t n = b.size();
    switch(target) {
        case 0: { GN3d::sTEXTURE_IMAGE img; GN3d::decodePngMemory(d, n, img); break; }
        case 1: { GN3d::sTEXTURE_IMAGE img; GN3d::decodeJpegMemory(d, n, img); break; }
        case 2: { GN3d::sASSET a; a.loadFromMemory(d, n); break; }
        case 3: { mmd::PmdModel m; std::string e; mmd::loadPmdStream(d, n, m, &e); break; }
        case 4: { mmd::PmxModel m; std::string e; mmd::loadPmxStream(d, n, m, &e); break; }
        case 5: {
            if(n < 4) break; // 内存模式需 memsize>=eFILE_MEM(3)
            unsigned short fmt=0, ch=0, bit=0; unsigned int freq=0, sz=0;
            uint8_t* data = GNa::LoadWavFileData((char*)d, &fmt, &ch, &bit, &freq, &sz, (int)n);
            if(data) delete[] data;
            break;
        }
    }
}

std::vector<uint8_t> seedFor(int target) {
    std::vector<uint8_t> s;
    auto put = [&](const char* p, size_t n){ s.insert(s.end(), p, p+n); };
    switch(target) {
        case 0: { // PNG 签名 + IHDR
            const uint8_t sig[] = {0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a};
            s.insert(s.end(), sig, sig+8);
            const uint8_t ihdr[] = {0,0,0,13,'I','H','D','R',0,0,0,2,0,0,0,2,8,6,0,0,0};
            s.insert(s.end(), ihdr, ihdr+sizeof(ihdr));
            break;
        }
        case 1: { const uint8_t j[] = {0xff,0xd8,0xff,0xe0,0,0x10,'J','F','I','F',0}; s.insert(s.end(), j, j+sizeof(j)); break; }
        case 2: { const char* g = "{\"asset\":{\"version\":\"2.0\"},\"scenes\":[{\"nodes\":[]}],\"nodes\":[]}"; put(g, strlen(g)); break; }
        case 3: { const char* p = "Pmd"; put(p,3); s.push_back(0); s.push_back(0); s.push_back(0); s.push_back(0x3f); s.push_back(0x80); break; }
        case 4: { const char* p = "PMX "; put(p,4); s.push_back(0); s.push_back(0); s.push_back(0); s.push_back(0x3f); s.push_back(0x80); break; }
        case 5: { // 最小 WAV 头（44 字节，显式数组避免 hex 转义吞字符）
            const uint8_t w[] = {'R','I','F','F',0x24,0,0,0,'W','A','V','E','f','m','t',' ',
                0x10,0,0,0, 1,0, 1,0, 0x44,0xac,0,0, 0x88,0x58,1,0, 2,0, 0x10,0,
                'd','a','t','a',0,0,0,0};
            s.insert(s.end(), w, w+sizeof(w)); break;
        }
    }
    return s;
}

void mutate(std::vector<uint8_t>& b) {
    if(b.empty()) { b.resize(1 + rndN(64)); for(auto& x : b) x = (uint8_t)rnd(); return; }
    int op = rndN(8);
    switch(op) {
        case 0: { size_t i = rndN((uint32_t)b.size()); b[i] ^= (uint8_t)(1 << rndN(8)); break; }
        case 1: { size_t i = rndN((uint32_t)b.size()); b[i] = (uint8_t)rnd(); break; }
        case 2: { if(b.size() > 1) b.erase(b.begin() + rndN((uint32_t)b.size())); break; }
        case 3: { size_t i = rndN((uint32_t)b.size()+1); b.insert(b.begin()+i, (uint8_t)rnd()); break; }
        case 4: { if(b.size() > 2) b.resize(1 + rndN((uint32_t)b.size()-1)); break; }
        case 5: { size_t i = rndN((uint32_t)b.size()); b[i] = (uint8_t)(b[i] + rndN(16) - 8); break; }
        case 6: { if(b.size() >= 2){ size_t i=rndN((uint32_t)b.size()-1); size_t j=rndN((uint32_t)b.size()-1); std::swap(b[i],b[j]); } break; }
        case 7: { size_t add = 1 + rndN(32); for(size_t k=0;k<add;k++) b.push_back((uint8_t)rnd()); break; }
    }
    if(b.size() > 65536) b.resize(65536);
}

} // namespace

int main(int argc, char** argv) {
    const char* secsEnv = getenv("FUZZ_SECS");
    long secs = secsEnv ? atol(secsEnv) : 3600;
    const char* seedEnv = getenv("FUZZ_SEED");
    if(seedEnv) g_rng = strtoull(seedEnv, nullptr, 10) | 1;
    const char* artEnv = getenv("FUZZ_ARTIFACT");
    if(artEnv) g_artifact = artEnv;
    // 确保 artifact 目录存在
    std::string dir = g_artifact.substr(0, g_artifact.find_last_of('/'));
    if(!dir.empty()) { std::string cmd = "mkdir -p '" + dir + "'"; system(cmd.c_str()); }

    std::vector<std::vector<uint8_t>> corpus;
    for(int t = 0; t < 6; t++) corpus.push_back(seedFor(t));
    // 可选外部 corpus 目录
    if(argc > 1) {
        std::string cmd = std::string("ls '") + argv[1] + "' 2>/dev/null";
        // 简单读入目录内文件
        FILE* p = popen(cmd.c_str(), "r");
        if(p) {
            char name[512];
            while(fgets(name, sizeof(name), p)) {
                name[strcspn(name, "\n")] = 0;
                std::string path = std::string(argv[1]) + "/" + name;
                FILE* f = fopen(path.c_str(), "rb");
                if(f) {
                    std::vector<uint8_t> b; int c;
                    while((c = fgetc(f)) != EOF && b.size() < 65536) b.push_back((uint8_t)c);
                    fclose(f);
                    if(!b.empty()) corpus.push_back(b);
                }
            }
            pclose(p);
        }
    }

    time_t start = time(nullptr);
    long iters = 0;
    fprintf(stderr, "[fuzz] start: %zu seeds, budget %lds, artifact %s\n", corpus.size(), secs, g_artifact.c_str());
    while(time(nullptr) - start < secs) {
        for(int t = 0; t < 6; t++) {
            std::vector<uint8_t> b = corpus[rndN((uint32_t)corpus.size())];
            int nm = 1 + rndN(4);
            for(int k = 0; k < nm; k++) mutate(b);
            writeArtifact(b);
            dispatch(b, t);
            iters++;
            // 保留部分变异样本扩充 corpus
            if(corpus.size() < 4096 && (rnd() & 63) == 0) corpus.push_back(b);
        }
        if((iters % 60000) == 0)
            fprintf(stderr, "[fuzz] %ld iters, %lds elapsed, corpus %zu\n", iters, (long)(time(nullptr)-start), corpus.size());
    }
    fprintf(stderr, "[fuzz] DONE: %ld iters in %lds, no crash\n", iters, (long)(time(nullptr)-start));
    return 0;
}
