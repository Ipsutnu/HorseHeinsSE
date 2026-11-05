# HorseReins — Skyrim SE/AE

Adaptacao non-VR de VR Reins, de SollyStones. Licenca GPL-3.0-or-later: veja LICENSE.
Bibliotecas, commits e requisitos: [README-BIBLIOTECAS.md](README-BIBLIOTECAS.md).

## Instalar
Instale HorseReins.zip com MO2/Vortex. Requer SKSE64, Address Library correspondente
ao executavel e SKSE Menu Framework 3. Desative a antiga Redeas.dll ao atualizar.
Meshes e texturas mantem os caminhos VRReins por compatibilidade com o NIF.

## Usar
- Cavalos novos recebem redea ao segurar Leather Strips com E junto a cabeca por
  0,6 segundo. Consome uma unidade do objeto segurado, sem exigir item no inventario.
- Opcional: HorseReins/Controls > Cavalos ja vem com redea (desativado por padrao).
- Mire num cavalo equipado proximo e segure G por 0,5 segundo para pegar/desamarrar.
- Segure G com a redea na mao para mover a ponta a frente; solte perto de uma
  superficie valida para amarrar. O contato usa a ponta simulada e margem de 32.
- Teclado ou gamepad podem ser selecionados no menu. O INI usa os codigos SKSE.
- A redea continua na mao ao montar e desmontar. A IA de acompanhamento fica
  suspensa durante a montaria e e retomada ao desmontar.

## Controle do cavalo
Na mao, o acompanhamento anterior foi mantido: considera movimento do jogador,
distancia entre os corpos e espaco junto ao focinho; mantem a caminhada lenta,
a parada quando o jogador para e a rotacao manual.
Ao amarrar, cancela o acompanhamento anterior e pede um acompanhamento nativo
ate o jogador por 1,5 segundo, com distancia temporaria de 180 unidades.
Em seguida aplica a ordem original: SetDontMove(true), SetRestrained(true),
ClearKeepOffsetFromActor e um unico EvaluatePackage. A aproximacao usa uma
ordem cancelavel; nao deixa PathToReference pendente depois do bloqueio.
Durante essa aproximacao nao forca rotacao, velocidade ou estado de locomocao.
Depois do bloqueio nao aplica EvaluatePackage novamente. A cada 0,5 segundo,
verifica sinais de locomocao: somente se detectar tentativa de movimento, aplica
StopMoving e limpa os sinais direcionais/corrida. Isso evita caminhar no lugar
sem parar o movimento a cada frame nem alterar as animacoes de descanso.
Ao desamarrar, cancela a aproximacao pendente e libera os dois bloqueios.

## Persistencia e compatibilidade
Equipamento e amarracoes ficam no co-save SKSE. As amarracoes incluem posicao,
rotacao, celula e marcador persistente invisivel. A restauracao ao recarregar o
cavalo e a protecao contra deslocamentos externos foram preservadas.
Integracao opcional com H2Horse.esp/DES_HorseCallScript: suspende o acompanhamento
do Press H to Horse ao assumir o cavalo e trata a chamada pela tecla quando preso.
Desamarre os cavalos antes de remover o mod de um save.

## Protecao do pescoco
HorseReins/Neck Protection ajusta uma superficie continua aproximada durante
montaria. Posicao relativa a cernelha: X frente, Y direita, Z cima; raios sao
metade das dimensoes; inclinacao em graus. Contorno rosa, ativar/desativar,
restaurar e copiar valores estao disponiveis. Retome o jogo para ver ajustes.
Padrao: NeckShape=-6,0,9.25,57,28.75,29.75,61.25.
A corda tem 150 unidades por lado, espessura fixa e transicoes suavizadas.
Ponta flutuante: alcance de 65 em primeira pessoa e 85 em terceira pessoa.
A protecao nao e colisao por triangulo da mesh; os apoios experimentais antigos
foram removidos. A corda nao arrasta fisicamente o cavalo.

## Idiomas e notificacoes
HorseReins/Translations: EN, PTBR, RU, ZH simplificado, JA, FR, ES, PL, DE e IT.
Caracteres ausentes na fonte do menu viram ?. As fontes opcionais sao mantidas;
a limpeza usa a API segura do framework. Fontes do HUD do Skyrim tambem precisam
conter os caracteres escolhidos. Notificacoes podem ser desligadas em Controls.
Preferencias sao salvas em Data/SKSE/Plugins/HorseReins.ini.

## Compilar e verificar
Visual Studio 2022 com C++ Desktop e Windows SDK; xmake >=3.0; Git; C++23.
Execute .\build.ps1. Saida: build/windows/x64/releasedbg/HorseReins.dll e HorseReins.zip.
Se faltar memoria, execute xmake -j 1 -y dentro da pasta do projeto e rode o script
novamente para empacotar. A primeira compilacao pode baixar dependencias.
A verificacao numerica da corda fica em tests/rope_check.cpp:
xmake -P tests e xmake run -P tests rope_check. Nao substitui os testes no jogo.
