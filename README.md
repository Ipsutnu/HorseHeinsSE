# HorseReins

Rédeas com física para conduzir e amarrar cavalos em **Skyrim Special Edition e Anniversary Edition**, sem VR.

## Requisitos

- SKSE64 compatível com a versão do jogo.
- Address Library for SKSE Plugins, na versão correspondente ao executável.
- SKSE Menu Framework 3.

## Instalação

Instale `HorseReins.zip` pelo Mod Organizer 2 ou Vortex. Ao atualizar de uma versão antiga chamada Redeas, desative `Redeas.dll`.

## Como usar

1. **Equipar o cavalo:** segure uma Leather Strip com **E** e mantenha-a próxima à cabeça do cavalo por um breve momento. Uma unidade será consumida.
2. **Pegar ou desamarrar a rédea:** mire no cavalo equipado e segure **G** por meio segundo.
3. **Conduzir:** caminhe com a rédea na mão. Ela permanece com o personagem ao montar e desmontar.
4. **Amarrar:** segure **G** para posicionar a ponta da corda e solte junto a uma superfície válida.

O equipamento e as amarrações são salvos entre sessões e mudanças de célula. Desamarre os cavalos antes de remover o mod de um save.

## Configurações

Abra **HorseReins** no SKSE Menu Framework.

| Aba | Opções |
| --- | --- |
| **Controls** | Tecla ou botão de gamepad, notificações e rédeas automáticas para todos os cavalos. |
| **Translations** | Idioma dos menus e notificações. |
| **Neck Protection** | Ajuste da proteção que mantém a corda sobre o pescoço durante a montaria. |
| **Rope Endpoint** | Posição da ponta em terceira pessoa. **Default** usa a posição flutuante original; **Hand** posiciona a ponta junto à mão. |

As configurações são salvas em `Data/SKSE/Plugins/HorseReins.ini`. Rédeas automáticas vêm desativadas por padrão.

Idiomas disponíveis: inglês, português brasileiro, russo, chinês simplificado, japonês, francês, espanhol, polonês, alemão e italiano.

## Créditos e licença

Adaptado de **VR Reins**, de **SollyStones**. Distribuído sob a licença [GPL-3.0-or-later](LICENSE).

## Compilação

Requer Windows x64, Visual Studio 2022 com ferramentas C++ e Windows SDK, Git e xmake 3.0 ou superior.

Na pasta do projeto:

```powershell
git submodule update --init --recursive
xmake f -p windows -a x64 -m releasedbg -y
xmake -y
```

A DLL é gerada em `build/windows/x64/releasedbg/HorseReins.dll`. As dependências adicionais são obtidas pelo xmake. Os arquivos de instalação e os pacotes distribuídos são mantidos separadamente deste repositório de código-fonte.
