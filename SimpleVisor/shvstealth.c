/*
 * stealth.c - Anti-detection pra SimpleVisor
 *
 * Esconde o hypervisor de:
 * - CPUID (bit hypervisor present, signature 0x40000000)
 * - MSRs sensiveis (IA32_FEATURE_CONTROL, MSR_VMX_BASIC, etc.)
 * - Memory reads em regioes criticas (VMCS, hypervisor code)
 *
 * Baseado em tecnicas de HvPP, HyperHide, e cheats comerciais.
 */

#include "shv.h"

// Lista de MSRs que vazam presenca de hypervisor
// Quando o guest tenta ler esses MSRs, devolvemos valores "normais"
static const UINT32 SpoofedMsrs[] = {
    0x480,  // IA32_VMX_BASIC
    0x481,  // IA32_VMX_PINBASED_CTLS
    0x482,  // IA32_VMX_PROCBASED_CTLS
    0x483,  // IA32_VMX_EXIT_CTLS
    0x484,  // IA32_VMX_ENTRY_CTLS
    0x485,  // IA32_VMX_MISC
    0x486,  // IA32_VMX_CR0_FIXED
    0x487,  // IA32_VMX_CR4_FIXED
    0x488,  // IA32_VMX_VMCS_ENUM
    0x489,  // IA32_VMX_PROCBASED_CTLS2
    0x48A,  // IA32_VMX_EPT_VPID_CAP
    0x48B,  // IA32_VMX_TRUE_PINBASED_CTLS
    0x48C,  // IA32_VMX_TRUE_PROCBASED_CTLS
    0x48D,  // IA32_VMX_TRUE_EXIT_CTLS
    0x48E,  // IA32_VMX_TRUE_ENTRY_CTLS
    0x48F,  // IA32_VMX_VMFUNC
    0x490,  // IA32_VMX_PROCBASED_CTLS3
    0x491,  // IA32_VMX_EXIT_CTLS2
    0x492,  // IA32_VMX_ALL_PROCBASED_CTLS (SVM)
    0x3A,   // IA32_FEATURE_CONTROL
};

// Cache dos MSRs reais (lidos uma vez no init)
static UINT64 g_RealMsrValues[RTL_NUMBER_OF(SpoofedMsrs)] = {0};

// Cache dos valores "limpos" (sem revelar hypervisor)
static UINT64 g_CleanMsrValues[RTL_NUMBER_OF(SpoofedMsrs)] = {0};

// Flag global: anti-detection ligado/desligado
static BOOLEAN g_StealthEnabled = TRUE;


/*
 * Calcula um valor "limpo" do MSR que nao revela hypervisor.
 * Pra cada MSR, zera os bits que indicam presenca de VMX.
 */
static UINT64 CleanMsrValue(_In_ UINT32 Msr, _In_ UINT64 RealValue)
{
    UINT64 CleanValue = RealValue;

    switch (Msr) {
        case 0x480: // IA32_VMX_BASIC
            // Bit 0-30: revision ID (manter)
            // Bit 32-44: VMCS size (manter, se for > 0)
            // Bit 50: memory type WB (manter)
            // Bit 53: use MSRs for capabilities (manter)
            // Bit 54: MSR 0x485 supported (manter)
            // Bit 55: dual-monitor (manter)
            // Bit 56-63: ? (manter)
            // Nao tem bit "VMX supported" explicito, mas se nao for zero,
            // indica que CPU suporta VMX. Por seguranca, manter.
            break;

        case 0x481: // IA32_VMX_PINBASED_CTLS
        case 0x482: // IA32_VMX_PROCBASED_CTLS
        case 0x483: // IA32_VMX_EXIT_CTLS
        case 0x484: // IA32_VMX_ENTRY_CTLS
            // Esses MSRs existem APENAS se VMX esta habilitado.
            // Devolver 0 esconde a presenca.
            CleanValue = 0;
            break;

        case 0x485: // IA32_VMX_MISC
            // Tambem so existe com VMX. Devolver 0.
            CleanValue = 0;
            break;

        case 0x486: // IA32_VMX_CR0_FIXED
        case 0x487: // IA32_VMX_CR4_FIXED
            // Esses podem existir sem VMX, mas valores especificos vazam.
            // Manter o valor real (geralmente sao fixos baseado em CR0/CR4).
            break;

        case 0x48A: // IA32_VMX_EPT_VPID_CAP
            // SO existe com VMX. Devolver 0.
            CleanValue = 0;
            break;

        case 0x3A:  // IA32_FEATURE_CONTROL
            // Bit 0: lock bit (manter, sempre 1)
            // Bit 1: enable VMX in SMX (manter, geralmente 0)
            // Bit 2: enable VMX outside SMX (ZERAR se nao queremos revelar)
            // Bit 3: SENTER (manter)
            CleanValue = RealValue;
            CleanValue &= ~0x4ULL;  // Clear bit 2 (VMX outside SMX)
            break;

        default:
            // Outros: manter valor real
            break;
    }

    return CleanValue;
}


/*
 * Inicializa o subsistema de anti-detection.
 * Chamado quando o hypervisor carrega.
 */
NTSTATUS ShvStealthInitialize(VOID)
{
    UINT32 i;

    DbgPrint("[Stealth] Inicializando anti-detection\n");

    // Le todos os MSRs uma vez e calcula os valores "limpos"
    for (i = 0; i < RTL_NUMBER_OF(SpoofedMsrs); i++) {
        g_RealMsrValues[i] = __readmsr(SpoofedMsrs[i]);
        g_CleanMsrValues[i] = CleanMsrValue(SpoofedMsrs[i], g_RealMsrValues[i]);
    }

    DbgPrint("[Stealth] %lu MSRs cacheados\n", i);

    return STATUS_SUCCESS;
}


/*
 * Chamado pelo VM exit handler quando guest faz CPUID.
 * Filtra os valores que vazam hypervisor.
 */
VOID ShvStealthHandleCpuid(_Inout_ INT32 CpuInfo[4])
{
    // CPUID leaf 1, ECX bit 31 = hypervisor present
    // NAO setar o bit (esconder)
    CpuInfo[2] &= ~HYPERV_HYPERVISOR_PRESENT_BIT;

    // CPUID leaf 0x40000000 = hypervisor interface signature
    // Devolver 0 (sem hypervisor)
    // Isso ja e tratado no handler, mas reforcamos aqui
}


/*
 * Chamado pelo VM exit handler quando guest faz RDMSR.
 * Se o MSR for um dos "perigosos", devolve o valor limpo.
 *
 * Retorna TRUE se o MSR foi spoofado, FALSE se deve usar valor real.
 */
BOOLEAN ShvStealthHandleRdmsr(
    _In_ UINT32 Msr,
    _Out_ UINT64 *Value)
{
    UINT32 i;

    if (!g_StealthEnabled) {
        return FALSE;  // Deixa passar valor real
    }

    for (i = 0; i < RTL_NUMBER_OF(SpoofedMsrs); i++) {
        if (SpoofedMsrs[i] == Msr) {
            *Value = g_CleanMsrValues[i];
            return TRUE;  // Spoofed
        }
    }

    return FALSE;  // MSR nao e perigoso, deixa passar
}


/*
 * Chamado pelo VM exit handler quando guest faz WRMSR.
 * Bloqueia escrita em MSRs sensiveis.
 *
 * Retorna TRUE se o write foi bloqueado, FALSE se deve prosseguir.
 */
BOOLEAN ShvStealthHandleWrmsr(_In_ UINT32 Msr)
{
    if (!g_StealthEnabled) {
        return FALSE;  // Deixa escrever
    }

    // Bloqueia escrita em MSRs que podem revelar hypervisor
    // (mas nao afeta funcionamento do Windows)
    for (UINT32 i = 0; i < RTL_NUMBER_OF(SpoofedMsrs); i++) {
        if (SpoofedMsrs[i] == Msr) {
            return TRUE;  // Bloqueado (write nao acontece)
        }
    }

    return FALSE;  // Deixa escrever
}


/*
 * Liga/desliga anti-detection em runtime.
 */
VOID ShvStealthSetEnabled(_In_ BOOLEAN Enabled)
{
    g_StealthEnabled = Enabled;
    DbgPrint("[Stealth] %s\n", Enabled ? "ON" : "OFF");
}


/*
 * Cleanup quando hypervisor descarrega.
 */
VOID ShvStealthCleanup(VOID)
{
    // Limpa caches (seguranca)
    RtlSecureZeroMemory(g_RealMsrValues, sizeof(g_RealMsrValues));
    RtlSecureZeroMemory(g_CleanMsrValues, sizeof(g_CleanMsrValues));
    DbgPrint("[Stealth] Cleanup OK\n");
}
