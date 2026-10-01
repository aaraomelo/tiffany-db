#ifndef TIFFANY_SHELL_H
#define TIFFANY_SHELL_H
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#define tiffany_shell_existe(p) (_access((p), 0) == 0)
#else
#include <unistd.h>
#define tiffany_shell_existe(p) (access((p), X_OK) == 0)
#endif

/* Ingere os binários shell no motor. Espelho de tools/env_*.bat.
 * Não é o PATH do utilizador a autoridade. */

static int tiffany_tenta(char *buf, size_t cap, const char *path){
    if(!path || !*path) return 0;
    if(!tiffany_shell_existe(path)) return 0;
    snprintf(buf, cap, "%s", path);
    return 1;
}

static const char *tiffany_node_bin(void){
    static char buf[512];
    const char *v = getenv("TIFFANY_NODE");
    if(v && *v) return v;
#ifdef _WIN32
    {
        const char *nb = getenv("NODE_BIN");
        if(nb && *nb){
            snprintf(buf, sizeof buf, "%s\\node.exe", nb);
            if(tiffany_shell_existe(buf)) return buf;
        }
        const char *pf = getenv("ProgramFiles");
        if(pf && *pf){
            snprintf(buf, sizeof buf, "%s\\nodejs\\node.exe", pf);
            if(tiffany_shell_existe(buf)) return buf;
        }
        const char *la = getenv("LOCALAPPDATA");
        if(la && *la){
            snprintf(buf, sizeof buf,
                "%s\\Programs\\cursor\\resources\\app\\resources\\helpers\\node.exe", la);
            if(tiffany_shell_existe(buf)) return buf;
        }
    }
#endif
    return "node";
}

static const char *tiffany_bash_bin(void){
    static char buf[512];
    const char *v = getenv("TIFFANY_BASH");
    if(v && *v) return v;
#ifdef _WIN32
    {
        const char *bb = getenv("BASH_BIN");
        if(bb && *bb){
            snprintf(buf, sizeof buf, "%s\\bash.exe", bb);
            if(tiffany_shell_existe(buf)) return buf;
        }
        const char *pf = getenv("ProgramFiles");
        const char *px = getenv("ProgramFiles(x86)");
        const char *la = getenv("LOCALAPPDATA");
        if(pf && *pf){
            snprintf(buf, sizeof buf, "%s\\Git\\usr\\bin\\bash.exe", pf);
            if(tiffany_shell_existe(buf)) return buf;
            snprintf(buf, sizeof buf, "%s\\Git\\bin\\bash.exe", pf);
            if(tiffany_shell_existe(buf)) return buf;
        }
        if(px && *px){
            snprintf(buf, sizeof buf, "%s\\Git\\usr\\bin\\bash.exe", px);
            if(tiffany_shell_existe(buf)) return buf;
            snprintf(buf, sizeof buf, "%s\\Git\\bin\\bash.exe", px);
            if(tiffany_shell_existe(buf)) return buf;
        }
        if(la && *la){
            snprintf(buf, sizeof buf, "%s\\Programs\\Git\\usr\\bin\\bash.exe", la);
            if(tiffany_shell_existe(buf)) return buf;
            snprintf(buf, sizeof buf, "%s\\Programs\\Git\\bin\\bash.exe", la);
            if(tiffany_shell_existe(buf)) return buf;
        }
        if(tiffany_tenta(buf, sizeof buf, "C:\\msys64\\usr\\bin\\bash.exe")) return buf;
    }
#else
    if(tiffany_tenta(buf, sizeof buf, "/usr/bin/bash")) return buf;
    if(tiffany_tenta(buf, sizeof buf, "/bin/bash")) return buf;
#endif
    return "bash";
}

static const char *tiffany_pwsh_bin(void){
    static char buf[512];
    const char *v = getenv("TIFFANY_PWSH");
    if(v && *v) return v;
    v = getenv("TIFFANY_POWERSHELL");
    if(v && *v) return v;
#ifdef _WIN32
    {
        const char *pf = getenv("ProgramFiles");
        const char *sr = getenv("SystemRoot");
        if(pf && *pf){
            snprintf(buf, sizeof buf, "%s\\PowerShell\\7\\pwsh.exe", pf);
            if(tiffany_shell_existe(buf)) return buf;
            snprintf(buf, sizeof buf, "%s\\PowerShell\\pwsh.exe", pf);
            if(tiffany_shell_existe(buf)) return buf;
        }
        if(sr && *sr){
            snprintf(buf, sizeof buf,
                "%s\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", sr);
            if(tiffany_shell_existe(buf)) return buf;
        }
        return "powershell";
    }
#else
    if(tiffany_tenta(buf, sizeof buf, "/usr/bin/pwsh")) return buf;
    return "pwsh";
#endif
}

/* Hospedeiros opcionais (PTX/LLVM/GLSL/provas). Se o binário não existir,
 * o pleno da casa continua a valer (emissor wasm na arena). */
static const char *tiffany_host_opcional(const char *env){
    const char *v = getenv(env);
    if(v && *v && tiffany_shell_existe(v)) return v;
    return NULL;
}
static const char *tiffany_ptx_bin(void){ return tiffany_host_opcional("TIFFANY_PTX"); }
static const char *tiffany_llc_bin(void){ return tiffany_host_opcional("TIFFANY_LLC"); }
static const char *tiffany_glsl_bin(void){ return tiffany_host_opcional("TIFFANY_GLSL"); }
static const char *tiffany_ghc_bin(void){ return tiffany_host_opcional("TIFFANY_GHC"); }
static const char *tiffany_dafny_bin(void){ return tiffany_host_opcional("TIFFANY_DAFNY"); }
static const char *tiffany_isabelle_bin(void){ return tiffany_host_opcional("TIFFANY_ISABELLE"); }

#endif
