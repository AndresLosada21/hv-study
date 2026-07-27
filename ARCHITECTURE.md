# Arquitetura Completa + Plano de Produção
# Hypervisor-based CS2 Toolkit

## Status Atual: ~5-10%

O que foi feito:
- [x] SimpleVisor clonado e entendido
- [x] CPUID stealth (esconde bit hypervisor)
- [x] MSR stealth (esconde MSRs de VMX)
- [x] Hyper-V signature zerada
- [x] Integração no driver (init/cleanup)

O que falta: ~90-95%

---

## 1. ARQUITETURA COMPLETA

```
┌─────────────────────────────────────────────────────────────────┐
│                        USER MODE (Ring 3)                       │
│                                                                 │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────────┐ │
│  │  CS2.exe    │  │  Overlay UI  │  │  Config Manager        │ │
│  │  (guest)    │  │  (ImGui)     │  │  (JSON/INI)            │ │
│  └──────┬──────┘  └──────┬───────┘  └───────────┬────────────┘ │
│         │                │                       │              │
│         │ VM exit        │ IOCTL                 │ IOCTL        │
│         ▼                ▼                       ▼              │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              DRIVER (Ring 0) - shv.sys                   │   │
│  │                                                          │   │
│  │  ┌────────────────────────────────────────────────────┐  │   │
│  │  │           HYPERVISOR CORE (Ring -1)                │  │   │
│  │  │                                                    │  │   │
│  │  │  ┌──────────┐  ┌──────────┐  ┌──────────────────┐ │  │   │
│  │  │  │ VMX Init │  │ VMCS Mgr │  │ VM Exit Handler  │ │  │   │
│  │  │  │ (VMXON)  │  │ (R/W)    │  │ (dispatch)       │ │  │   │
│  │  │  └──────────┘  └──────────┘  └────────┬─────────┘ │  │   │
│  │  │                                       │           │  │   │
│  │  │  ┌────────────────────────────────────┼─────────┐ │  │   │
│  │  │  │         ANTI-DETECTION LAYER       │         │ │  │   │
│  │  │  │                                    ▼         │ │  │   │
│  │  │  │  ┌─────────┐ ┌─────────┐ ┌──────────────┐  │ │  │   │
│  │  │  │  │ CPUID   │ │  MSR    │ │   Timing     │  │ │  │   │
│  │  │  │  │ Stealth │ │ Stealth │ │   Stealth    │  │ │  │   │
│  │  │  │  │ [FEITO] │ │ [FEITO] │ │   [TODO]     │  │ │  │   │
│  │  │  │  └─────────┘ └─────────┘ └──────────────┘  │ │  │   │
│  │  │  │  ┌─────────┐ ┌─────────┐ ┌──────────────┐  │ │  │   │
│  │  │  │  │ Memory  │ │ Driver  │ │  Signature   │  │ │  │   │
│  │  │  │  │ Stealth │ │ Stealth │ │  Stealth     │  │ │  │   │
│  │  │  │  │ [TODO]  │ │ [TODO]  │ │  [TODO]      │  │ │  │   │
│  │  │  │  └─────────┘ └─────────┘ └──────────────┘  │ │  │   │
│  │  │  └────────────────────────────────────────────┘ │  │   │
│  │  │                                                 │  │   │
│  │  │  ┌────────────────────────────────────────────┐ │  │   │
│  │  │  │         EPT HOOK ENGINE                    │ │  │   │
│  │  │  │                                            │ │  │   │
│  │  │  │  ┌──────────┐ ┌──────────┐ ┌────────────┐ │ │  │   │
│  │  │  │  │  Page    │ │  Shadow  │ │   Hook     │ │ │  │   │
│  │  │  │  │  Hook    │ │  Pages   │ │   Manager  │ │ │  │   │
│  │  │  │  │  [TODO]  │ │  [TODO]  │ │   [TODO]   │ │ │  │   │
│  │  │  │  └──────────┘ └──────────┘ └────────────┘ │ │  │   │
│  │  │  └────────────────────────────────────────────┘ │  │   │
│  │  │                                                 │  │   │
│  │  │  ┌────────────────────────────────────────────┐ │  │   │
│  │  │  │         MEMORY ACCESS LAYER                │ │  │   │
│  │  │  │                                            │ │  │   │
│  │  │  │  ┌──────────┐ ┌──────────┐ ┌────────────┐ │ │  │   │
│  │  │  │  │  Page    │ │  Read/   │ │  Process   │ │ │  │   │
│  │  │  │  │  Walk    │ │  Write   │ │  Memory    │ │ │  │   │
│  │  │  │  │  [TODO]  │ │  [TODO]  │ │  [TODO]    │ │ │  │   │
│  │  │  │  └──────────┘ └──────────┘ └────────────┘ │ │  │   │
│  │  │  └────────────────────────────────────────────┘ │  │   │
│  │  │                                                 │  │   │
│  │  │  ┌────────────────────────────────────────────┐ │  │   │
│  │  │  │         SYSCALL PROXY                      │ │  │   │
│  │  │  │                                            │ │  │   │
│  │  │  │  ┌──────────┐ ┌──────────┐ ┌────────────┐ │ │  │   │
│  │  │  │  │  EPT     │ │  Direct  │ │  Syscall   │ │ │  │   │
│  │  │  │  │  Hook    │ │  Syscall │ │  Table     │ │ │  │   │
│  │  │  │  │  ntdll   │ │  [TODO]  │ │  [TODO]    │ │ │  │   │
│  │  │  │  │  [TODO]  │ │          │ │            │ │ │  │   │
│  │  │  │  └──────────┘ └──────────┘ └────────────┘ │ │  │   │
│  │  │  └────────────────────────────────────────────┘ │  │   │
│  │  └─────────────────────────────────────────────────┘  │   │
│  │                                                       │   │
│  │  ┌─────────────────────────────────────────────────┐  │   │
│  │  │         GAME INTERFACE                          │  │   │
│  │  │                                                 │  │   │
│  │  │  ┌──────────┐ ┌──────────┐ ┌────────────────┐  │  │   │
│  │  │  │ Offsets  │ │  Entity  │ │  View Matrix   │  │  │   │
│  │  │  │ (dumper) │ │  List    │ │  + Bones       │  │  │   │
│  │  │  │ [TODO]   │ │  [TODO]  │ │  [TODO]        │  │  │   │
│  │  │  └──────────┘ └──────────┘ └────────────────┘  │  │   │
│  │  └─────────────────────────────────────────────────┘  │   │
│  │                                                       │   │
│  │  ┌─────────────────────────────────────────────────┐  │   │
│  │  │         CHEAT LOGIC                             │  │   │
│  │  │                                                 │  │   │
│  │  │  ┌──────────┐ ┌──────────┐ ┌────────────────┐  │  │   │
│  │  │  │ Aimbot   │ │   ESP    │ │  Triggerbot    │  │  │   │
│  │  │  │ [TODO]   │ │  [TODO]  │ │  [TODO]        │  │  │   │
│  │  │  └──────────┘ └──────────┘ └────────────────┘  │  │   │
│  │  └─────────────────────────────────────────────────┘  │   │
│  │                                                       │   │
│  │  ┌─────────────────────────────────────────────────┐  │   │
│  │  │         HWID SPOOFER                            │  │   │
│  │  │                                                 │  │   │
│  │  │  ┌──────────┐ ┌──────────┐ ┌────────────────┐  │  │   │
│  │  │  │  Disk    │ │   MAC    │ │  BIOS UUID     │  │  │   │
│  │  │  │  Serial  │ │  Address │ │  + Volume      │  │  │   │
│  │  │  │  [TODO]  │ │  [TODO]  │ │  [TODO]        │  │  │   │
│  │  │  └──────────┘ └──────────┘ └────────────────┘  │  │   │
│  │  └─────────────────────────────────────────────────┘  │   │
│  └───────────────────────────────────────────────────────┘   │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │         ANTI-OVERWATCH (heuristica de jogo)             │ │
│  │                                                         │ │
│  │  ┌──────────┐ ┌──────────┐ ┌────────────────────────┐  │ │
│  │  │ Movement │ │ Reaction │ │  Aim Error Injection   │  │ │
│  │  │ Humanize │ │ Time Var │ │  [TODO]                │  │ │
│  │  │ [TODO]   │ │ [TODO]   │ │                        │  │ │
│  │  └──────────┘ └──────────┘ └────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. FLUXO DE DADOS (runtime)

```
CS2.exe renderiza frame
       │
       ▼
[VAC check] ──CPUID──► Hypervisor intercepta
       │                    │
       │                    ▼
       │              Devolve "sem hypervisor"
       │                    │
       ▼                    ▼
[VAC check] ──RDMSR──► Hypervisor intercepta
       │                    │
       │                    ▼
       │              Devolve MSR "limpo"
       │                    │
       ▼                    ▼
[VAC check] ──ReadProcessMemory──► VAC hook intercepta
       │                              │
       │                              ▼
       │                        Syscall Proxy
       │                              │
       │                              ▼
       │                        Hypervisor faz read direto
       │                              │
       │                              ▼
       │                        Devolve dados ao VAC
       │                              │
       ▼                              ▼
VAC: "tudo limpo"              VAC: "tudo limpo"
       │
       ▼
CS2.exe continua rodando
       │
       ▼
[Hypervisor lê estado do jogo via EPT]
       │
       ▼
[Game Interface: offsets → entity data]
       │
       ▼
[Aimbot: calcula alvo + smoothing]
       │
       ▼
[Input: SendInput/mouse_event]
       │
       ▼
CS2.exe recebe input
```

---

## 3. PLANO DE PRODUÇÃO

### Fase 0: Ambiente (1 semana)
**Status: PENDENTE**

- [ ] VM com nested virtualization (VMware/VirtualBox)
- [ ] Windows 10/11 na VM
- [ ] Visual Studio 2022 + WDK 10
- [ ] WinDbg + VirtualKD (debug kernel)
- [ ] Compilar SimpleVisor original (sem modificações)
- [ ] Testar: `sc start shv` → hypervisor roda
- [ ] Testar: `sc stop shv` → hypervisor para

**Entrega:** Ambiente funcional, SimpleVisor compilando e rodando.

---

### Fase 1: Anti-detection básico (2-3 semanas)
**Status: ~80% FEITO**

- [x] CPUID stealth (bit 31 ECX = 0)
- [x] MSR stealth (RDMSR/WRMSR filtered)
- [x] Hyper-V signature zerada (0x40000000)
- [x] Integração no driver (init/cleanup)
- [ ] Timing stealth (TSC, QPC, RDTSC)
- [ ] Memory stealth (esconder código do hypervisor)
- [ ] Driver stealth (nome genérico, sem strings)
- [ ] Testar com CPU-Z (não detecta hypervisor)
- [ ] Testar com VAC offline (não detecta)

**Entrega:** Hypervisor invisível pra CPUID/MSR.

**Risco:** VAC pode ter checks adicionais não documentados.

---

### Fase 2: EPT Hook Engine (3-4 semanas)
**Status: PENDENTE**

- [ ] Page hooking (split TLB)
  - Mapear página original + página shadow
  - EPT violation quando guest acessa
  - Devolver shadow (modificada) ou original
- [ ] Shadow page management
  - Alocar páginas shadow
  - Copiar conteúdo original
  - Aplicar modificações
- [ ] Hook manager
  - Registrar hooks (VA → shadow page)
  - Desregistrar hooks
  - Listar hooks ativos
- [ ] Testar com DLL dummy
  - Hookar página de código
  - Verificar que guest vê shadow
  - Verificar que "verificador" vê original

**Entrega:** Sistema de EPT hook funcional.

**Dependência:** Fase 1 (anti-detection).

**Complexidade:** ALTA. EPT hook é o coração de tudo.

---

### Fase 3: Memory Access Layer (2-3 semanas)
**Status: PENDENTE**

- [ ] Page walk (VA → PA)
  - Traduzir endereço virtual do guest pra físico
  - Usar CR3 do guest + EPT
- [ ] Read/Write via hypervisor
  - Ler memória física do guest
  - Escrever memória física do guest
- [ ] Process memory access
  - Abrir processo (via hypervisor, bypass hooks)
  - Ler memória de processo específico
  - Escrever memória de processo específico
- [ ] Testar com notepad.exe
  - Ler string da janela
  - Modificar string
  - Verificar que notepad vê modificação

**Entrega:** Acesso completo à memória do guest.

**Dependência:** Fase 2 (EPT hook).

---

### Fase 4: Game Interface (2-3 semanas)
**Status: PENDENTE**

- [ ] cs2-dumper offsets
  - Rodar cs2-dumper pra gerar offsets
  - Parsear output (JSON/C++)
  - Atualizar offsets quando CS2 atualiza
- [ ] Entity list reading
  - Ler lista de entidades (jogadores, bots)
  - Filtrar por tipo (player, bot, etc.)
- [ ] Local player
  - Ler estado do jogador local
  - Vida, posição, ângulo, arma
- [ ] View matrix + bones
  - Ler matriz de projeção
  - Ler posições de bones (cabeça, peito, etc.)
  - Calcular posição 2D na tela
- [ ] Testar offline (mapa de treino)
  - Verificar que lê entidades corretamente
  - Verificar que calcula posição 2D

**Entrega:** Interface completa com CS2.

**Dependência:** Fase 3 (memory access).

**Risco:** Offsets mudam a cada update do CS2.

---

### Fase 5: Aimbot básico (2-3 semanas)
**Status: PENDENTE**

- [ ] Target selection
  - Selecionar alvo mais próximo
  - Filtrar por FOV
  - Priorizar cabeça
- [ ] Aim calculation
  - Calcular ângulo necessário
  - Aplicar smoothing (não snap instantâneo)
  - Predição de movimento
- [ ] Input injection
  - SendInput (mouse movement)
  - mouse_event (fallback)
- [ ] Testar offline
  - Verificar que mira em bots
  - Verificar smoothing

**Entrega:** Aimbot funcional offline.

**Dependência:** Fase 4 (game interface).

---

### Fase 6: Syscall Proxy (2-3 semanas)
**Status: PENDENTE**

- [ ] EPT hook na ntdll.dll
  - Hookar funções: NtOpenProcess, NtReadVirtualMemory, etc.
  - Quando VAC chama, hypervisor intercepta
- [ ] Direct syscall
  - Fazer syscall direto (bypass hook)
  - Usar `syscall` instruction
  - Devolver resultado ao caller
- [ ] Syscall table management
  - Mapear syscall numbers
  - Atualizar quando Windows atualiza
- [ ] Testar com VAC ativo (offline)
  - Verificar que VAC não detecta
  - Verificar que syscalls funcionam

**Entrega:** Bypass completo de hooks VAC.

**Dependência:** Fase 2 (EPT hook).

**Complexidade:** ALTA. Syscall numbers mudam entre builds do Windows.

---

### Fase 7: HWID Spoofer (2 semanas)
**Status: PENDENTE**

- [ ] Disk serial
  - Hookar IOCTL_STORAGE_QUERY_PROPERTY
  - Devolver serial random
- [ ] MAC address
  - Hookar IOCTL_NDIS_QUERY_GLOBAL_STATS
  - Devolver MAC random
- [ ] BIOS UUID
  - Hookar SMBIOS table
  - Devolver UUID random
- [ ] Volume serial
  - Hookar GetVolumeInformation
  - Devolver serial random
- [ ] Testar
  - Verificar que HWID muda
  - Verificar que VAC não detecta

**Entrega:** HWID spoofing funcional.

**Dependência:** Fase 2 (EPT hook).

---

### Fase 8: Driver Signing (1 semana + R$ 500-2000)
**Status: PENDENTE**

- [ ] Comprar EV code signing cert
  - DigiCert, Sectigo, SSL.com
  - R$ 500-2000/ano
- [ ] Assinar driver com SignTool
  - `signtool sign /f cert.pfx /p password shv.sys`
- [ ] Instalar cert em Trusted Publishers
- [ ] Testar
  - Verificar que Windows aceita driver
  - Verificar que VAC aceita como trusted

**Entrega:** Driver assinado e confiável.

**Dependência:** Nenhuma (pode ser feito em paralelo).

**Custo:** R$ 500-2000/ano.

---

### Fase 9: Anti-Overwatch (constante)
**Status: PENDENTE**

- [ ] Movement humanization
  - Adicionar ruído ao movimento
  - Curvas bezier (não linha reta)
  - Velocidade variável
- [ ] Reaction time variation
  - Randomizar tempo de reação (150-300ms)
  - Não reagir instantaneamente
- [ ] Aim error injection
  - Errar propositalmente (5-10% dos tiros)
  - Não mirar sempre na cabeça
- [ ] Play pattern randomization
  - Não jogar sempre igual
  - Variar estratégia
- [ ] Testar em replays
  - Verificar que parece humano
  - Verificar que Overwatch não flagga

**Entrega:** Gameplay "humano".

**Dependência:** Fase 5 (aimbot).

**Complexidade:** EXTREMA. Overwatch é análise manual.

---

### Fase 10: Polish (2 semanas)
**Status: PENDENTE**

- [ ] UI overlay (ImGui)
  - Menu de configuração
  - ESP rendering
  - Stats display
- [ ] Config system
  - Salvar/carregar configs
  - Hotkeys
- [ ] Error handling
  - Try/catch em tudo
  - Logging detalhado
  - Recovery de erros
- [ ] Logging
  - Debug log (opcional)
  - Performance metrics
  - Error tracking

**Entrega:** Produto "final".

**Dependência:** Todas as fases anteriores.

---

## 4. TIMELINE TOTAL

```
Fase 0:  Ambiente              ████ 1 sem
Fase 1:  Anti-detection        ████████ 2-3 sem  [80% FEITO]
Fase 2:  EPT Hook Engine       ████████████ 3-4 sem
Fase 3:  Memory Access         ████████ 2-3 sem
Fase 4:  Game Interface        ████████ 2-3 sem
Fase 5:  Aimbot básico         ████████ 2-3 sem
Fase 6:  Syscall Proxy         ████████ 2-3 sem
Fase 7:  HWID Spoofer          ██████ 2 sem
Fase 8:  Driver Signing        ███ 1 sem (+ custo)
Fase 9:  Anti-Overwatch        ████████████████ constante
Fase 10: Polish                ██████ 2 sem
                               ─────────────────────────────
                               Total: 6-9 meses
```

**Com 4h/dia de estudo + implementação:** ~9-12 meses
**Com 8h/dia (full-time):** ~6-8 meses

---

## 5. RISCOS E MITIGAÇÃO

| Risco | Probabilidade | Impacto | Mitigação |
|---|---|---|---|
| VAC detecta hypervisor | Alta | Ban permanente | Anti-detection completo (Fase 1) |
| VAC detecta EPT hook | Média | Ban permanente | EPT hook indetectável (Fase 2) |
| VAC detecta syscall proxy | Média | Ban permanente | Syscall direto (Fase 6) |
| Overwatch flagga gameplay | Alta | Ban permanente | Anti-Overwatch (Fase 9) |
| CS2 atualiza offsets | Alta | Cheat quebra | cs2-dumper automático (Fase 4) |
| Windows atualiza syscalls | Média | Cheat quebra | Syscall table dinâmica (Fase 6) |
| BSOD/crash | Média | Perda de dados | Testar em VM (Fase 0) |
| EV cert expira | Baixa | Driver não carrega | Renovar anualmente (Fase 8) |

---

## 6. CUSTO TOTAL

| Item | Custo |
|---|---|
| Visual Studio 2022 | Grátis (Community) |
| WDK 10 | Grátis |
| VM (VMware/VirtualBox) | Grátis |
| EV Code Signing Cert | R$ 500-2000/ano |
| Tempo (6-9 meses) | Seu tempo |
| **Total financeiro** | **R$ 500-2000** |

---

## 7. O QUE FAZER AGORA

**Próximo passo imediato:**

1. **Compilar o que tem** (Fase 1)
   - Instalar Visual Studio + WDK
   - Compilar SimpleVisor modificado
   - Testar em VM

2. **Quando funcionar, seguir pra Fase 2** (EPT Hook Engine)
   - Esse é o componente mais crítico
   - Sem EPT hook, nada funciona

3. **Fase 2 é o divisor de águas**
   - Se conseguir fazer EPT hook funcionar, o resto é "só" implementação
   - Se não conseguir, o projeto para aqui

---

## 8. REFERÊNCIAS

- SimpleVisor: https://github.com/ionescu007/SimpleVisor
- HvPP: https://github.com/wbenny/hvpp
- HyperHide: https://github.com/Air14/HyperHide
- cs2-dumper: https://github.com/a2x/cs2-dumper
- Intel SDM Vol 3C (VMX): https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- Windows Internals (Russinovich): livro

---

## 9. NOTA FINAL

Esse documento é o mapa completo. Não é um tutorial passo-a-passo.
Cada fase precisa de estudo + implementação + teste.

O que foi feito até agora (Fase 1, 80%) é a fundação.
Sem as Fases 2-6, não tem cheat funcional.
Sem as Fases 7-10, não tem produto "completo".

**O caminho é longo. Mas é possível.**
