/*
 * shvstealth.c - Anti-detection para o SimpleVisor
 *
 * Esconde a presenca do hypervisor filtrando MSRs sensiveis de VMX:
 * leitura (RDMSR) devolve valor "limpo", escrita (WRMSR) e bloqueada.
 * A filtragem de CPUID fica em shvvmxhv.c.
 *
 * Este arquivo roda em hypervisor mode e usa apenas os headers locais
 * (shv.h), sem depender de ntifs.h/wdm.h.
 *
 * PENDENTE: para estes handlers rodarem de fato, os bits desses MSRs
 * precisam estar programados na MSR bitmap do VMCS. Hoje a bitmap fica
 * zerada (nenhum MSR gera VM-exit). Ver ShvVmxSetupVmcsForVp em shvvmx.c.
 */

#include "shv.h"

#define FEATURE_CONTROL_MSR                 0x3A
#define FEATURE_CONTROL_VMX_OUTSIDE_SMX     0x4

#define SHV_STEALTH_MSR_COUNT   (sizeof(SpoofedMsrs) / sizeof(SpoofedMsrs[0]))

//
// MSRs que revelam a presenca de VMX (valores conforme Intel SDM e vmx.h).
// Leitura: devolvemos valor "limpo". Escrita: bloqueamos.
//
static const UINT32 SpoofedMsrs[] =
{
    0x480,                  // IA32_VMX_BASIC
    0x481,                  // IA32_VMX_PINBASED_CTLS
    0x482,                  // IA32_VMX_PROCBASED_CTLS
    0x483,                  // IA32_VMX_EXIT_CTLS
    0x484,                  // IA32_VMX_ENTRY_CTLS
    0x485,                  // IA32_VMX_MISC
    0x486,                  // IA32_VMX_CR0_FIXED0
    0x487,                  // IA32_VMX_CR0_FIXED1
    0x488,                  // IA32_VMX_CR4_FIXED0
    0x489,                  // IA32_VMX_CR4_FIXED1
    0x48A,                  // IA32_VMX_VMCS_ENUM
    0x48B,                  // IA32_VMX_PROCBASED_CTLS2
    0x48C,                  // IA32_VMX_EPT_VPID_CAP
    0x48D,                  // IA32_VMX_TRUE_PINBASED_CTLS
    0x48E,                  // IA32_VMX_TRUE_PROCBASED_CTLS
    0x48F,                  // IA32_VMX_TRUE_EXIT_CTLS
    0x490,                  // IA32_VMX_TRUE_ENTRY_CTLS
    0x491,                  // IA32_VMX_VMFUNC
    FEATURE_CONTROL_MSR,    // IA32_FEATURE_CONTROL
};

//
// Valores "limpos" calculados no init. Nao guardamos os valores reais:
// para todos os MSRs de VMX devolvemos 0, exceto FEATURE_CONTROL, que
// devolve o valor real com o bit "VMX outside SMX" limpo.
//
static UINT64 g_CleanMsrValues[sizeof(SpoofedMsrs) / sizeof(SpoofedMsrs[0])];

//
// Flag global: anti-detection ligado/desligado.
//
static UINT8 g_StealthEnabled = TRUE;

//
// Inicializa o subsistema de anti-detection.
// Chamado uma vez, antes do hypervisor subir nos processadores.
//
INT32
ShvStealthInitialize (
    VOID
    )
{
    UINT32 i;
    UINT64 featureControl;

    ShvOsDebugPrint("[Stealth] Inicializando anti-detection\n");

    //
    // FEATURE_CONTROL (0x3A) existe em qualquer CPU Intel moderna, entao
    // a leitura e segura. Nao lemos os MSRs 0x48x: varios deles nao existem
    // dependendo da CPU e um RDMSR invalido causa #GP (BSOD no load).
    //
    featureControl = __readmsr(FEATURE_CONTROL_MSR);

    for (i = 0; i < SHV_STEALTH_MSR_COUNT; i++)
    {
        if (SpoofedMsrs[i] == FEATURE_CONTROL_MSR)
        {
            //
            // Mantem lock bit/SMX, limpa o bit "VMX outside SMX".
            //
            g_CleanMsrValues[i] =
                featureControl & ~((UINT64)FEATURE_CONTROL_VMX_OUTSIDE_SMX);
        }
        else
        {
            //
            // MSRs de VMX so existem com VMX presente: devolvemos 0.
            //
            g_CleanMsrValues[i] = 0;
        }
    }

    return SHV_STATUS_SUCCESS;
}

//
// Cleanup quando o hypervisor descarrega.
//
VOID
ShvStealthCleanup (
    VOID
    )
{
    UINT32 i;

    for (i = 0; i < SHV_STEALTH_MSR_COUNT; i++)
    {
        g_CleanMsrValues[i] = 0;
    }

    ShvOsDebugPrint("[Stealth] Cleanup OK\n");
}

//
// Liga/desliga anti-detection em runtime.
//
VOID
ShvStealthSetEnabled (
    _In_ UINT8 Enabled
    )
{
    g_StealthEnabled = Enabled;
    ShvOsDebugPrint("[Stealth] %s\n", Enabled ? "ON" : "OFF");
}

//
// Chamado pelo VM exit handler quando o guest faz RDMSR.
// Retorna TRUE (e o valor limpo) se o MSR foi spoofado,
// FALSE se o valor real pode ser usado.
//
UINT8
ShvStealthHandleRdmsr (
    _In_ UINT32 Msr,
    _Out_ PUINT64 Value
    )
{
    UINT32 i;

    if (g_StealthEnabled == FALSE)
    {
        return FALSE;
    }

    for (i = 0; i < SHV_STEALTH_MSR_COUNT; i++)
    {
        if (SpoofedMsrs[i] == Msr)
        {
            *Value = g_CleanMsrValues[i];
            return TRUE;
        }
    }

    return FALSE;
}

//
// Chamado pelo VM exit handler quando o guest faz WRMSR.
// Retorna TRUE se o write foi bloqueado, FALSE se pode prosseguir.
//
UINT8
ShvStealthHandleWrmsr (
    _In_ UINT32 Msr
    )
{
    UINT32 i;

    if (g_StealthEnabled == FALSE)
    {
        return FALSE;
    }

    for (i = 0; i < SHV_STEALTH_MSR_COUNT; i++)
    {
        if (SpoofedMsrs[i] == Msr)
        {
            return TRUE;
        }
    }

    return FALSE;
}
