/* banda.h — a banda, o keystream e o bump. Extraidos do canal.c: o grupo usa os mesmos,
 * porque p2p e pNp sao a mesma coisa com mais ouvidos. */
#ifndef BANDA_H
#define BANDA_H
#include <stdint.h>
#include <string.h>
/* o sha256 do martelo, o mesmo — a banda e sha256(tecido) */
static const unsigned K256[64] = {
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
#define ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
/* A COMPRESSAO, um bloco de 64. Estava enterrada dentro do sha256; sai para fora porque e ela
 * que permite STREAMAR — e streamar e o que tira o objeto todo da memoria. */
static void sha_bloco(unsigned *h, const unsigned char *b){
    unsigned w[64];
    for(int i=0;i<16;i++) w[i]=((unsigned)b[4*i]<<24)|((unsigned)b[4*i+1]<<16)|
                               ((unsigned)b[4*i+2]<<8)|b[4*i+3];
    for(int i=16;i<64;i++){
        unsigned s0=ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3);
        unsigned s1=ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1; }
    unsigned a=h[0],bb=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    for(int i=0;i<64;i++){
        unsigned S1=ROR(e,6)^ROR(e,11)^ROR(e,25), ch=(e&f)^(~e&g);
        unsigned t1=hh+S1+ch+K256[i]+w[i];
        unsigned S0=ROR(a,2)^ROR(a,13)^ROR(a,22), mj=(a&bb)^(a&c)^(bb&c);
        unsigned t2=S0+mj;
        hh=g;g=f;f=e;e=d+t1;d=c;c=bb;bb=a;a=t1+t2; }
    h[0]+=a;h[1]+=bb;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
}
static void sha_ini(unsigned *h){
    h[0]=0x6a09e667u;h[1]=0xbb67ae85u;h[2]=0x3c6ef372u;h[3]=0xa54ff53au;
    h[4]=0x510e527fu;h[5]=0x9b05688cu;h[6]=0x1f83d9abu;h[7]=0x5be0cd19u;
}
static void sha_fim(unsigned *h, unsigned char *out){
    for(int i=0;i<8;i++) for(int j=0;j<4;j++) out[4*i+j]=(unsigned char)(h[i]>>(24-8*j));
}
static void sha256(const unsigned char *m, size_t n, unsigned char *out){
    unsigned h[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                   0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    size_t tot=((n+9+63)/64)*64; unsigned char b[1024];
    if(tot>sizeof b) return;
    memset(b,0,tot); memcpy(b,m,n); b[n]=0x80;
    uint64_t bits=(uint64_t)n*8;
    for(int i=0;i<8;i++) b[tot-1-i]=(unsigned char)(bits>>(8*i));
    for(size_t o=0;o<tot;o+=64){
        unsigned w[64];
        for(int i=0;i<16;i++) w[i]=((unsigned)b[o+4*i]<<24)|((unsigned)b[o+4*i+1]<<16)|
                                   ((unsigned)b[o+4*i+2]<<8)|b[o+4*i+3];
        for(int i=16;i<64;i++){
            unsigned s0=ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3);
            unsigned s1=ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1; }
        unsigned a=h[0],bb=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for(int i=0;i<64;i++){
            unsigned S1=ROR(e,6)^ROR(e,11)^ROR(e,25), ch=(e&f)^(~e&g);
            unsigned t1=hh+S1+ch+K256[i]+w[i];
            unsigned S0=ROR(a,2)^ROR(a,13)^ROR(a,22), mj=(a&bb)^(a&c)^(bb&c);
            unsigned t2=S0+mj;
            hh=g;g=f;f=e;e=d+t1;d=c;c=bb;bb=a;a=t1+t2; }
        h[0]+=a;h[1]+=bb;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh; }
    for(int i=0;i<8;i++) for(int j=0;j<4;j++) out[4*i+j]=(unsigned char)(h[i]>>(24-8*j));
}

/* A BANDA: a assinatura do tecido. */
static void banda_de(const char *tecido, unsigned char *banda){
    sha256((const unsigned char*)tecido, strlen(tecido), banda);
}
/* Chave pública (hex) → banda = sha256(bytes). Mesmo bump; não é segundo protocolo.
 * ECDH X25519 (papers) quando há par; o URL só traz a pública — projecção sha256. */
static int banda_hexval(char c){
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int banda_de_chave(const char *hex, unsigned char *banda){
    unsigned char raw[256];
    size_t n = 0;
    const char *p;
    if(!hex || !banda) return 0;
    for(p = hex; *p; p++){
        int hi, lo;
        if(*p == ' ' || *p == '\n' || *p == '\r' || *p == ':') continue;
        hi = banda_hexval(*p);
        if(hi < 0 || !p[1]) return 0;
        lo = banda_hexval(*++p);
        if(lo < 0 || n >= sizeof raw) return 0;
        raw[n++] = (unsigned char)((hi << 4) | lo);
    }
    if(!n) return 0;
    sha256(raw, n, banda);
    return 1;
}
/* O KEYSTREAM: a banda esticada pelo comprimento da mensagem — sha256(banda||contador). */
static void keystream(const unsigned char *banda, unsigned char *ks, size_t n){
    unsigned char sem[36];
    memcpy(sem, banda, 32);
    for(size_t o = 0; o < n; o += 32){
        for(int k = 0; k < 4; k++) sem[32+k] = (unsigned char)((o/32) >> (8*k));
        unsigned char bloco[32];
        sha256(sem, 36, bloco);
        size_t r = n - o; if(r > 32) r = 32;
        memcpy(ks + o, bloco, r);
    }
}
/* O BUMP: msg XOR keystream. É J — a MESMA operação nos dois sentidos. */
static void bump(const unsigned char *ent, const unsigned char *ks, unsigned char *sai, size_t n){
    for(size_t i = 0; i < n; i++) sai[i] = ent[i] ^ ks[i];
}

#endif
