# SimpleVisor + Stealth (Anti-detection)

Hypervisor Intel VT-x baseado no SimpleVisor do Alex Ionescu, com modificações
pra esconder a presença do hypervisor de anti-cheats (VAC, EAC, BattlEye).

## O que tem aqui

Base: [SimpleVisor](https://github.com/ionescu007/SimpleVisor) (~500 LOC)
Modificações: arquivos em `SimpleVisor/`

**Arquivos novos:**
- `SimpleVisor/shvstealth.c` - Módulo de anti-detection

**Arquivos modificados:**
- `SimpleVisor/shvvmxhv.c` - VM exit handler agora esconde hypervisor
- `SimpleVisor/shv.c` - Inicializa/limpa stealth no load/unload
- `SimpleVisor/shv.h` - Declara funções do stealth
- `SimpleVisor/shv.vcxproj` - shvstealth.c adicionado ao build

## O que o stealth faz (Fase 1)

### 1. CPUID - Esconde hypervisor present bit

**Antes** (SimpleVisor original):
```c
if (Rax == 1) {
    CpuInfo[2] |= HYPERV_HYPERVISOR_PRESENT_BIT;  // REVELA hypervisor
}
else if (Rax == 0x40000000) {
    CpuInfo[0] = ' vhS';  // REVELA signature "SimpleVisor"
}
```

**Depois** (nossa modificação):
```c
if (Rax == 1) {
    CpuInfo[2] &= ~HYPERV_HYPERVISOR_PRESENT_BIT;  // ESCONDE
}
else if (Rax == 0x40000000 || Rax == 0x40000001) {
    CpuInfo[0] = CpuInfo[1] = CpuInfo[2] = CpuInfo[3] = 0;  // ZERA
}
```

VAC faz `CPUID` e checa bit 31 de ECX. Se setado, tem hypervisor.
Nossa modificação deixa o bit 0 (não tem hypervisor).

### 2. MSR - Esconde registradores sensíveis

Intercepta `RDMSR` (exit reason 31) e `WRMSR` (exit reason 32).
MSRs que vazam hypervisor:
- `IA32_VMX_BASIC` (0x480) - existe só com VMX
- `IA32_VMX_PROCBASED_CTLS` (0x482) - idem
- `IA32_VMX_EPT_VPID_CAP` (0x48A) - idem
- `IA32_FEATURE_CONTROL` (0x3A) - bit 2 = VMX outside SMX

Quando guest tenta ler, devolvemos 0 (não existe VMX).
Quando guest tenta escrever em MSR sensivel, bloqueia.

**Nota (pendente):** pros handlers de RDMSR/WRMSR rodarem de fato, os bits
desses MSRs precisam estar programados na MSR bitmap do VMCS. Hoje a bitmap
fica zerada (nenhum MSR gera VM-exit). Falta programar em `ShvVmxSetupVmcsForVp`.

### 3. Hypervisor signature

Leaf `0x40000000` da CPUID normalmente retorna signature do hypervisor
(`Microsoft Hv`, `VMwareVMware`, `KVMKVMKVM`, etc.).

Devolvemos 0 (sem hypervisor).

## Como compilar

### 1. Instalar Visual Studio 2019/2022 com Workload "Desktop development with C++"

Inclui MSVC compiler, Windows SDK, MSBuild.

### 2. Instalar Windows Driver Kit (WDK) 10

Baixa de: https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk

### 3. Habilitar test signing na VM

```cmd
bcdedit /set testsigning on
bcdedit /set nointegritychecks on
```

Reinicia a VM.

### 4. Compilar o projeto

```cmd
cd C:\hypervisor-study\SimpleVisor
msbuild shv.sln /p:Configuration="Windows NT" /p:Platform=x64 /p:WindowsTargetPlatformVersion=10.0.28000.0
```

Gera `x64\NT Release\shv.sys`.

### 5. Carregar na VM (não no host!)

```cmd
sc create shv type= kernel binPath= "C:\path\shv.sys"
sc start shv
```

Verifica:
```cmd
sc query shv
```

Pra parar:
```cmd
sc stop shv
sc delete shv
```

## O que ainda falta (próximas fases)

### Fase 2: EPT hook (esconder DLL na memória)

Quando CS2 lê própria memória, vê "limpo".
Quando VAC lê, vê "limpo".

Arquivo a criar: `SimpleVisor/shvept_hook.c`
Modificar: `SimpleVisor/shvvp.c` (adicionar hook manager)

### Fase 3: Bypass de VAC hooks (syscall direto)

VAC hooka funções em ntdll.dll. Hypervisor intercepta syscalls
via EPT na ntdll e faz a chamada real.

Arquivo a criar: `SimpleVisor/shvsyscall.c`

### Fase 4: HWID spoofer (esconder hardware ID)

Driver que muda serial number, MAC, BIOS UUID antes do VAC ler.

Arquivo a criar: `SimpleVisor/shvhwid.c`

### Fase 5: Assinatura EV code signing

Cert EV real (R$ 500-2000/ano). Assina o .sys.
VAC aceita como trusted publisher.

### Fase 6: Anti-Overwatch (heuristica de jogo)

Reaction time variavel, aim com erro proposital,
movement humanizado. **Matematicamente o mais difícil**.

## Estrutura do hypervisor

```
CPU (Intel VT-x)
  └─ Hypervisor (nosso shv.sys)
       ├─ VMXON / VMCS / EPT
       ├─ VM Exit Handler (shvvmxhv.c)
       │    ├─ CPUID:  esconde hypervisor
       │    ├─ RDMSR:  esconde MSRs
       │    ├─ WRMSR:  bloqueia writes
       │    ├─ INVD/XSETBV: passam direto
       │    └─ EPT violation (futuro)
       ├─ Anti-detection (shvstealth.c)
       │    ├─ Cache de MSRs reais
       │    └─ Calculo de valores "limpos"
       └─ EPT Manager (shvvp.c)
            ├─ Identity map (atual)
            └─ Hook system (futuro)
```

## Testando

### Teste 1: Hypervisor carrega
```cmd
sc start shv
```
Esperado: `STATE: RUNNING`

### Teste 2: CPUID esconde hypervisor
Rodar em userland (PowerShell):
```powershell
$cpu = Get-WmiObject Win32_Processor
$cpuid = & "C:\Tools\cpuid.exe" 1  # se tiver CPUID-Z ou similar
# ECX bit 31 deve ser 0
```

### Teste 3: MSRs escondidos
```cmd
cpuz.exe  # CPU-Z não deve mostrar "VMX supported"
```

## Aviso

Esse código modifica registradores de CPU e instala um hypervisor.
Risco de BSOD, corrupção de dados, e detecção por anti-cheat.

Use apenas em VM isolada pra estudo.
Não nos responsabilizamos por uso indevido.
