# eSpeak NG para ESP32

Síntese de voz em português europeu no ESP32, com o [eSpeak NG](https://github.com/espeak-ng/espeak-ng), sem ligação à Internet.

Esta adaptação nasceu no projecto CSD-AP (Canivete Suíço Digital, Assistente Pessoal), um dispositivo acessível para pessoas cegas com base no M5Stack CoreS3. Aqui está só a parte da voz, pronta a usar noutros projectos ESP-IDF.

## O que tem

- **O eSpeak NG completo a correr no ESP32-S3.** Os dados da voz ficam na flash e são lidos directamente de lá, sem ocupar RAM.
- **A voz portuguesa (roa/pt) e 33 timbres**, de f1 a f5, de m1 a m7, Andrea, Diogo, Marco, Mario, pedro, entre outros.
- **Uma função simples para falar**: `espeak_tts_speak("Olá!")`. O áudio é entregue ao seu programa em mono, 16 bits, à taxa que escolher.
- **Números, datas e horas lidos como em português** ("18,5 graus", "terça-feira").
- **Letras acentuadas pronunciadas correctamente.** A biblioteca de C do ESP-IDF não as reconhece, por isso a adaptação inclui uma correcção para isso.
- **Um exemplo com um amplificador I2S** (MAX98357A ou parecido) e um teste no emulador QEMU, que corre sem placa.

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `components/espeak-ng` | Código do eSpeak NG (versão fixa em `UPSTREAM_COMMIT`), com as alterações para ler os dados da flash mapeada |
| `components/espeak_pack` | Leitura do pacote de dados na partição `espeak` |
| `components/espeak_tts` | Tarefa de síntese e funções para falar, mudar o timbre, a velocidade e a voz |
| `data/espeak-pt.bin` | Pacote de dados já compilado: português e timbres (cerca de 810 KB) |
| `tools/espeak` | Programas para recompilar os dados, acrescentar pronúncias ou outros idiomas |
| `examples/ola_mundo` | Exemplo com saída I2S |
| `tests/qemu` | Teste no emulador do ESP32-S3 |

## Requisitos

- ESP-IDF 5.4 (testado com a 5.4; deve funcionar com 5.2 ou mais recente).
- ESP32-S3 com pelo menos 4 MB de flash. Testado no M5Stack CoreS3 e no QEMU. PSRAM é opcional.
- Para ouvir: um amplificador I2S, por exemplo o MAX98357A, e um altifalante. Também pode usar um codec, como o AW88298 do CoreS3, ligando o sink ao driver do codec.

## Começar

1. Ligue o amplificador:
   - BCLK ao GPIO 5;
   - LRC ao GPIO 6;
   - DIN ao GPIO 7;
   - alimentação e massa.

   Os pinos mudam-se em `idf.py menuconfig`, no menu "Exemplo eSpeak: pinos do I2S".
2. Compile e grave:

   ```sh
   cd examples/ola_mundo
   idf.py set-target esp32s3
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

   `idf.py flash` grava também os dados da voz na partição `espeak`.
3. O ESP32 diz algumas frases, mostra quatro timbres e uma velocidade rápida, e depois conta de 5 em 5 segundos.

## Usar no seu projecto

1. Copie as pastas `components/espeak-ng`, `components/espeak_pack` e `components/espeak_tts` para o seu projecto, ou aponte para elas com `EXTRA_COMPONENT_DIRS`.
2. Acrescente a partição à `partitions.csv`, com pelo menos 1 MB:

   ```
   espeak,   data, 0x40,    0x210000, 0x100000,
   ```
3. No `CMakeLists.txt` do `main`, para que `idf.py flash` grave os dados:

   ```cmake
   esptool_py_flash_to_partition(flash "espeak" "${CMAKE_CURRENT_LIST_DIR}/../data/espeak-pt.bin")
   ```
4. No código:

   ```c
   #include "espeak_tts.h"

   static bool meu_sink(const int16_t *pcm, size_t n, void *ctx)
   {
       /* enviar n amostras para o I2S, codec, ficheiro WAV... */
       return true;            /* false interrompe a frase */
   }

   espeak_tts_config_t cfg = {
       .sink = meu_sink,
       .out_rate = 16000,      /* 0 ou 22050 = sem conversão */
       .voice = "roa/pt",
       .variant = "f3",        /* NULL = voz normal */
       .rate_wpm = 175,
   };
   espeak_tts_init(&cfg);
   espeak_tts_wait_ready(10000);
   espeak_tts_speak("Olá, mundo!");
   ```

### Funções

| Função | O que faz |
|---|---|
| `espeak_tts_init(&cfg)` | Monta os dados e arranca a tarefa do eSpeak |
| `espeak_tts_wait_ready(ms)` | Espera que o arranque termine (cerca de 20 ms) |
| `espeak_tts_speak(texto)` | Fala um texto em UTF-8 e espera que acabe |
| `espeak_tts_stop()` | Interrompe a frase em curso, a partir de outra tarefa |
| `espeak_tts_set_rate(ppm)` | Velocidade, de 80 a 450 palavras por minuto |
| `espeak_tts_set_variant(nome)` | Timbre: f1 a f5, m1 a m7, Andrea, Diogo, Marco... |
| `espeak_tts_set_voice(nome)` | Voz do eSpeak, se estiver no pacote (por exemplo gmw/en) |

O sink é chamado dentro da tarefa do eSpeak. Não chame `espeak_tts_speak` a partir dele.

## Memória

- **Flash:** o exemplo completo ocupa cerca de 560 KB (com o ESP-IDF), mais 810 KB de dados de voz.
- **RAM sem PSRAM:** cerca de 150 KB de RAM estática e uma pilha de 32 KB. Cabe no ESP32-S3.
- **RAM com PSRAM:** active `CONFIG_SPIRAM=y` e `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`. A RAM estática e a pilha passam para a PSRAM, e a RAM interna fica livre para o Wi-Fi, o Bluetooth e a câmara.
  - Nesse caso, a tarefa do eSpeak não pode escrever na flash (NVS, SPIFFS, FAT) a partir do sink. Passe o áudio a outra tarefa se precisar de o gravar.

## Mudar pronúncias, timbres ou idiomas

Os dados já vêm compilados em `data/espeak-pt.bin`. Para os mudar:

1. **Pronúncias:** edite `tools/espeak/pt_extra`, uma palavra por linha, seguida de um tab e dos fonemas do eSpeak. Para ver os fonemas de uma palavra, use `espeak-ng -v roa/pt -q -x "palavra"`.
2. **Timbres:** acrescente ou retire nomes na lista `VARS` de `tools/espeak/build_espeak_data.sh`.
3. Corra o script. Precisa de git, cmake e um compilador de C no computador.

   ```sh
   tools/espeak/build_espeak_data.sh
   ```

   O script descarrega o eSpeak NG na mesma versão, compila o dicionário português e gera um novo `data/espeak-pt.bin`.
4. Grave outra vez com `idf.py flash`.

**Outro idioma:** use `tools/espeak/make_espeak_pack.py` directamente. Acrescente o dicionário e o ficheiro da voz com `--extra`, por exemplo `--extra en_dict lang/gmw/en`, e aumente a partição se for preciso. Depois use `espeak_tts_set_voice("gmw/en")`.

`tools/espeak/patch_espeak.py` é o script que aplicou ao código do eSpeak NG as alterações de leitura da flash. Já está aplicado em `components/espeak-ng`. Só é preciso para actualizar o eSpeak para uma versão mais recente.

## Teste sem placa

```sh
python $IDF_PATH/tools/idf_tools.py install qemu-xtensa
tests/qemu/run.sh
```

O teste sintetiza cinco frases, com timbres e velocidades diferentes, e confirma que cada uma produz áudio com uma duração plausível. No fim deve aparecer `TESTE TUDO OK`.

## Licença

O eSpeak NG é software livre com a licença GNU GPL versão 3 ou posterior, e este repositório também, com o mesmo texto (`LICENSE`). Quem distribuir firmware com este código tem de disponibilizar também o código-fonte.

## Autoria

Adaptação ao ESP32: Domingos Guedes Ferreira, com a ajuda do Claude (Anthropic) para debug de erros.
eSpeak NG: Reece H. Dunn e colaboradores, a partir do eSpeak de Jonathan Duddington.
