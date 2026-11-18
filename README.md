# MicroSIP | Excel

Build interno do MicroSIP 3.22.16 (GPL v2) para a Excel Suprimentos.

## O que muda em relação ao oficial

- Português do Brasil embutido no executável (`res/langpack_portuguesebr.txt`). Um `langpack_*.txt`
  de outro idioma ao lado do .exe continua valendo; o `langpack_portuguesebr.txt` do instalador oficial é ignorado.
- Título, bandeja e versão do .exe: "MicroSIP | Excel". Pasta de dados e `MicroSIP.ini` continuam os do MicroSIP.
- Sem vídeo, sem verificação de atualização e sem envio de relatório de falha ao microsip.org.
- Lista de ramais do Deak à direita do discador (`Ramais.cpp`), zebrada, com filtro por nome ou ramal.
  Duplo clique (ou Enter) só preenche o número no discador; ligar é ação do usuário. O menu do botão
  direito tem Preencher discagem, Chamar, Transferir (em chamada) e Copiar ramal. Atualiza sozinha a cada 30 min.
- Conexão: com servidor no `MicroSIP.ini`, vale o `.ini`; sem ele, a que o ERP Deak grava em
  `HKCU\Software\VB and VBA Program Settings\DEAK Sistemas\Banco Dados` (`Nome Servidor`, `Nome Banco`,
  `User Id do Bando de Dados`, `Senha Banco de Dados`). Sem nenhuma das duas, a lista não aparece.
- Configurável em Configurações › "Lista de ramais" › "..." (`RamaisConfigDlg.cpp`).
  Usuário vazio = autenticação do Windows. Grava em `MicroSIP.ini`, seção `[Settings]`:
  `ramaisServer`, `ramaisDatabase`, `ramaisUser`, `ramaisPassword`. Padrão da Excel: login `deak`.
  A senha pode ir em texto puro num `.ini` distribuído; o programa a cifra na primeira abertura.
  Banco fora do ar não trava a abertura: a consulta roda em segundo plano com timeout de 5 s (login) e 15 s (consulta).

## Como compilar

Dependências em `D:\Projetos\MicroSIP-deps` (fora do SVN; outra pasta: variável `MICROSIP_DEPS`):

1. Visual Studio 2026 com "MSVC Build Tools x64/x86" e "MFC" (toolset v145).
2. `pjproject` 2.17 (`git clone --branch 2.17 https://github.com/pjsip/pjproject`) com
   `deps\config_site.h` copiado para `pjlib\include\pj\config_site.h` (sem vídeo, TLS pelo Schannel), compilado com:
   `MSBuild pjproject-vs14.sln /t:libpjproject /p:Configuration=Release-Static /p:Platform=Win32 /p:BuildToolset=v145 /p:PlatformToolset=v145`
3. `SQLiteCpp` 3.4.0 (`git clone --branch 3.4.0 https://github.com/SRombauts/SQLiteCpp`) e
   `deps\build-sqlitecpp.cmd` copiado para a raiz de `MicroSIP-deps` e executado lá.
4. `MSBuild microsip.vcxproj /p:Configuration=Release /p:Platform=Win32`
   → `D:\Projetos\MicroSIP-deps\out\Win32\Release\microsip.exe`

## Instalador

`instalador\MicroSIP-Excel.iss` (Inno Setup 6). Depois de compilar o .exe:
`"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" instalador\MicroSIP-Excel.iss`
→ `D:\Projetos\MicroSIP-deps\dist\MicroSIP-Excel-<versão>-<build>-setup.exe`. A cada nova entrega, subir `AppBuildExcel`
e copiar o instalador para `instalador\dist` (é o que vai no SVN para distribuir).

- Por usuário, sem administrador, em `%LOCALAPPDATA%\MicroSIP Excel`, como `MicroSIP.exe`, com os sons `.wav`.
- Aponta `HKCU\Software\MicroSIP` para essa pasta, então usa `%APPDATA%\MicroSIP\MicroSIP.ini` (mesma
  conta, contatos e histórico do oficial). Opções: atalho na área de trabalho e iniciar com o Windows.
- Recusa continuar com algum MicroSIP aberto (dois registrariam o mesmo ramal).
- Instalação silenciosa: `MicroSIP-Excel-...-setup.exe /VERYSILENT /TASKS="desktopicon,startup"`.

## Atenção na troca do MicroSIP oficial por este

A chave que cifra a senha SIP no `.ini` não vem no fonte publicado; este build não lê a senha gravada
pelo oficial. A senha da conta SIP precisa ser digitada de novo uma vez (ou o `.ini` distribuído com a
senha em texto puro, que é cifrada no primeiro uso).
 
## Seguranca e Auditoria Tecnica 
- Credenciais armazenadas de forma cifrada no MicroSIP.ini ou via autenticacao integrada do Windows (ERP Deak). 
- Sanitizacao de parametros de busca e validacao de entrada antes da consulta ao banco de dados. 
