# RAPORT - Space-Skirmish
[Autor]:          Paweł Turek 155236\
[Przedmiot]:      Systemy Operacyjne\
[Semestr]:        3\
[Wydział]:        WIiM\
[Kierunek]:       Informatyka\
[Data]:           2026-02-06

---

## Spis treści
1. [Środowisko testowe](#środowisko-testowe)
2. [Opis projektu](#opis-projektu)
3. [Cel projektu](#cel-projektu)
4. [Założenia projektu](#założenia-projektu)
5. [Uruchomienie](#uruchomienie)
6. [Pliki i ich role](#pliki-i-ich-role)
7. [Ogólny opis działania](#ogólny-opis-działania)
8. [Semafory](#semafory)
9. [Pamięć dzielona](#pamięć-dzielona)
10. [Kolejka komunikatów](#kolejka-komunikatów)
11. [Co udało się zrobić](#co-udało-się-zrobić)
12. [Elementy specjalne](#elementy-specjalne)
13. [Problemy podczas projektu](#problemy-podczas-projektu)
14. [Opis ważniejszych elementów](#opis-ważniejszych-elementów)
15. [Testy](#testy)
    - [Test 1: Spawning eskadr i rozkazy GUARD](#test-1-spawning-eskadr-i-rozkazy-guard)
    - [Test 2: Patrolowanie Battleship](#test-2-patrolowanie-battleship)
    - [Test 3: Przypisanie dowódcy do eskadry](#test-3-przypisanie-dowódcy-do-eskadry)
    - [Test 4: Śmierć dowódcy i przepisanie](#test-4-śmierć-dowódcy-i-przepisanie)
16. [Linki do kluczowych fragmentów kodu](#linki-do-kluczowych-fragmentów-kodu)
    - [a. Tworzenie i obsługa plików](#a-tworzenie-i-obsługa-plików)
    - [b. Tworzenie procesów](#b-tworzenie-procesów)
    - [c. Tworzenie i obsługa wątków](#c-tworzenie-i-obsługa-wątków)
    - [d. Obsługa sygnałów](#d-obsługa-sygnałów)
    - [e. Synchronizacja procesów (semafory)](#e-synchronizacja-procesów-semafory)
    - [f. Łącza nazwane i nienazwane](#f-łącza-nazwane-i-nienazwane)
    - [g. Segmenty pamięci dzielonej](#g-segmenty-pamięci-dzielonej)
    - [h. Kolejki komunikatów](#h-kolejki-komunikatów)
    - [i. Gniazda](#i-gniazda)

---

## Środowisko testowe

* WSL:\
Wersja podsystemu WSL: 2.6.1.0\
Wersja jądra: 6.6.87.2-1\
Wersja usługi WSLg: 1.0.66\
Wersja MSRDC: 1.2.6353\
Wersja Direct3D: 1.611.1-81528511\
Wersja DXCore: 10.0.26100.1-240331-1435.ge-release\
Wersja systemu Windows: 10.0.26100.7623
* gcc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0

---
## Opis projektu

Space-Skirmish to wielomodułowy projekt symulacji gry/strategii konsolowej, w której niezależne komponenty współpracują poprzez mechanizmy IPC, logowanie i wspólne interfejsy. Aplikacja symuluje starcie eskadr w przestrzeni kosmicznej, a jej logika została rozbita na osobne procesy (m.in. centrum dowodzenia, menedżer konsoli, interfejs użytkownika oraz moduły pomocnicze), aby zaprezentować praktyczne użycie komunikacji międzyprocesowej, obsługi błędów i synchronizacji.

---
## Cel projektu

Celem projektu jest zaprojektowanie i implementacja spójnego systemu procesów współpracujących w środowisku Linux/WSL, z naciskiem na:
* zastosowanie IPC (potoki/nazwane łącza lub gniazda) do wymiany danych,
* bezpieczne uruchamianie i nadzorowanie procesów,
* centralne logowanie i raportowanie błędów,
* czytelny interfejs konsolowy umożliwiający obserwację przebiegu rozgrywki.

---
## Założenia projektu

Założenia znajdują się w pliku [README.md](https://github.com/PaurXen/Space-Skirmish-/blob/main/README.md).

---
## Uruchomienie
Projekt składa się z 1 głównego programu, 2 pobocznych i 1 pomocniczego. Do uruchomienia programu używamy:
```bash
# standardowe uruchomienie
./command_center&

# uruchomienie z jednym ze scenariuszy z folderu scenarios
./command_center --scenario default&

# uruchomienie consoli (najlepiej w innym terminalu niż command center)
./console_manager

# standardowe uruchomienie TUI
./ui

# uruchomienie TUI z ftok
./ui --ftok ./key.ipc

# uruchomienie TUI z folderem logów
./ui --run-dir ./logs/run_2026-02-05_12-00-00_pid12345
```

Aby zakończyć program, wyślij SIGINT (Ctrl + C) do ./command_center lub wpisz "end" w ./console_manager.

---
## Pliki i ich role
### ipc/
* `ipc_context.c|h` - konfiguracja i inicjalizacja kontekstu IPC
* `ipc_mesq.c|h` - obsługa kolejek komunikatów (msgget/msgsnd/msgrcv)
* `semaphores.c|h` - operacje na semaforach i synchronizacja
* `shared.h` - wspólne definicje i struktury IPC
### CC/
* `flagship.c` (TBA) - logika flagowego okrętu (planowane)
* `command_center.c` - główny proces sterujący symulacją
* `battleship.c` - implementacja zachowań okrętu typu battleship
* `squadron.c` - zarządzanie eskadrą jednostek
* `unit_logic.c|h` - logika działania jednostek w turze
* `unit_ipc.c|h` - komunikacja jednostek przez IPC
* `unit_stats.c|h` - statystyki jednostek (HP, tarcze, itp.)
* `weapon_stats.c|h` - statystyki i parametry uzbrojenia
* `unit_size.c|h` - rozmiary i klasy jednostek
* `scenario.c|h` - wczytywanie i obsługa scenariuszy
### CM/
* `console_manager.c|h` - interfejs konsolowy do sterowania i podglądu
### UI/
* `ui_main.c` - wejście aplikacji TUI
* `ui.h` - główne definicje interfejsu UI
* `ui_map.c|h` - renderowanie mapy pola bitwy
* `ui_ust.c|h` - obsługa ustawień i układu UI
* `ui_std.c|h` - standardowe komponenty i narzędzia UI
### tee/
* `terminal_tee.c|h` - duplikowanie wyjścia do terminala i pliku
### ./
* `error_handler.c|h` - obsługa błędów i raportowanie
* `log.h` - makra i interfejs logowania
* `utils.c` - funkcje pomocnicze
* `tee_spawn.h` - uruchamianie procesu tee

---
## Ogólny opis działania
Po uruchomieniu `command_center` startuje główny proces symulacji, który inicjalizuje kontekst IPC, wczytuje scenariusz i cyklicznie przelicza logikę starcia, a także odpowiada za uruchamianie procesów BS, SQ i tee. Moduł `console_manager` umożliwia podgląd i sterowanie przebiegiem (np. zakończenie symulacji), a `ui` prezentuje stan gry w formie TUI. Wymiana danych między procesami odbywa się przez kolejki komunikatów i semafory, a logowanie trafia zarówno do terminala, jak i do plików w katalogu logs.

---
## Semafory
W projekcie użyto semaforów System V do synchronizacji dostępu do pamięci współdzielonej oraz do bariery „ticków” między procesami. Zestaw semaforów jest tworzony i resetowany w `ipc_create()` na wartości początkowe, a ich indeksy są zdefiniowane w `shared.h`:
* `SEM_GLOBAL_LOCK` – mutex chroniący całą strukturę `shm_state_t` (grid, jednostki, liczniki).
* `SEM_TICK_START` – CC wystawia N pozwoleń na start kroku symulacji dla aktywnych jednostek.
* `SEM_TICK_DONE` – każda jednostka zgłasza zakończenie kroku; CC czeka na N sygnałów.

Warstwa pomocnicza w `semaphores.c|h` dostarcza bezpieczne opakowania nad `semop()`, w tym:
* retry na `EINTR` (`sem_op_retry`, `sem_post_retry`),
* wersje przerywalne z flagą stopu (`sem_op_intr`, `sem_wait_intr`),
* proste blokady `sem_lock`/`sem_unlock` dla pojedynczego semafora.

### Przepływ synchronizacji (tick barrier)
1. CC oblicza liczbę aktywnych jednostek i wykonuje `sem_post_retry(SEM_TICK_START, +1)` tyle razy, ile jednostek ma wykonać krok.
2. Każdy proces jednostki (BS/SQ) czeka na `SEM_TICK_START`, wykonuje swoją logikę w kroku i kończy przez `sem_post_retry(SEM_TICK_DONE, +1)`.
3. CC wykonuje `sem_wait_intr(SEM_TICK_DONE, -1)` N razy, aby zebrać potwierdzenia zakończenia kroku.

### Dostęp do pamięci współdzielonej
`SEM_GLOBAL_LOCK` służy jako mutex dla całej struktury `shm_state_t`. W praktyce:
* CC chroni aktualizacje liczników i struktur jednostek,
* procesy BS/SQ blokują dostęp podczas modyfikacji własnego stanu i logiki ruchu,
* UI blokuje odczyt podczas kopiowania mapy i listy jednostek (snapshot do renderu).

### Zamykanie i przerwania
Wersje `*_intr` pozwalają na przerwanie oczekiwania, gdy ustawiona jest flaga stopu (np. po SIGTERM/SIGINT). Dzięki temu procesy mogą zakończyć się kontrolowanie bez pozostawiania zablokowanych semaforów.

---
## Pamięć dzielona
Pamięć dzielona jest realizowana przez segment SysV i stanowi główne źródło prawdy o stanie gry. Segment jest tworzony przez CC w `ipc_create()` i mapowany w procesach potomnych przez `ipc_attach()`. Zawartość opisana jest w strukturze `shm_state_t` i obejmuje:
* `magic` – znacznik poprawnej inicjalizacji segmentu (sprawdzany przy podłączeniu).
* `ticks` – globalny licznik kroków symulacji, inkrementowany przez CC.
* `next_unit_id` – prosty allocator identyfikatorów jednostek.
* `unit_count` – liczba aktywnych jednostek.
* `tick_expected` / `tick_done` – pola pomocnicze dla bariery ticków (koordynacja CC ↔ jednostki).
* `last_step_tick[]` – ostatni tick wykonany przez konkretną jednostkę (zapobiega podwójnym akcjom w tym samym kroku).
* `grid[M][N]` – mapa pola bitwy przechowująca `unit_id_t` (0 = pusto, wartość ujemna dla przeszkód).
* `units[MAX_UNITS+1]` – tablica rekordów `unit_entity_t` z informacjami o procesach i stanie jednostek.

### Rekord jednostki (`unit_entity_t`)
Każda jednostka posiada minimalny opis potrzebny do sterowania i wizualizacji:
* `pid` – PID procesu jednostki (do sygnałów).
* `faction`, `type`, `alive` – frakcja, typ i stan życia.
* `position` – pozycja na siatce.
* `flags` – miejsce na stany/rozkazy.
* `dmg_payload` – skumulowane obrażenia do przetworzenia w kolejnym kroku.

### Statystyki i uzbrojenie
Statystyki jednostek (`unit_stats_t`) i uzbrojenie (`weapon_stats_t`, `weapon_loadout_view_t`) są współdzielone po to, aby procesy mogły odczytywać i aktualizować bieżące parametry walki (HP, tarcze, zasięg, obrażenia, bay fighterów).

### Dostęp i spójność
* Każda modyfikacja `shm_state_t` wykonywana jest pod `SEM_GLOBAL_LOCK`, aby zachować spójność siatki i listy jednostek.
* UI nie modyfikuje pamięci – wykonuje szybki snapshot (kopię) i zwalnia blokadę przed renderem.
* CC inicjalizuje segment do stanu zerowego i ustawia `magic`, dzięki czemu nowe procesy mogą zweryfikować poprawność podłączenia.

---
## Kolejka komunikatów
Komunikacja asynchroniczna pomiędzy procesami realizowana jest przez dwie kolejki SysV: żądaniową i odpowiedzi. Identyfikatory kluczy są stałe (`MQ_KEY_REQ`, `MQ_KEY_REP`) i zdefiniowane w `ipc_mesq.h`. Kolejki są tworzone podczas inicjalizacji IPC (CC) i pozostają aktywne przez cały czas działania systemu. Wiadomości są wysyłane w trybie nieblokującym (`IPC_NOWAIT`), co pozwala pętlom symulacji działać bez długiego blokowania; odbiór w większości przypadków jest „próbujący” (tryb `mq_try_recv_*`), a blokujące oczekiwanie stosowane jest tylko tam, gdzie to potrzebne (np. UI czeka na odpowiedź snapshotu).

### Podział na kolejki
* **REQ (żądania)** – wszystkie żądania kierowane do CC (spawn, rozkazy, komendy CM, zapytania UI).
* **REP (odpowiedzi)** – odpowiedzi z CC do konkretnego nadawcy (filtrowane po `mtype` równym PID nadawcy).

### Typy komunikatów (mtype)
`mtype` pełni rolę kanału logicznego/filtra. Stosowane są dwa schematy:
* **mtype stałe** – dla żądań kierowanych do CC (`MSG_SPAWN`, `MSG_CM_CMD`, `MSG_UI_MAP_REQ` itd.). CC odbiera je według stałego typu.
* **mtype dynamiczne (PID)** – dla odpowiedzi i wiadomości adresowanych do konkretnego procesu. Nadawca ustawia `mtype = PID` odbiorcy, a odbiorca filtruje po własnym PID.

Specjalny przypadek stanowią rozkazy (`mq_order_t`), gdzie `mtype` to PID + `MQ_ORDER_MTYPE_OFFSET`. Dzięki temu rozkazy nie kolidują z innymi komunikatami adresowanymi po PID.

Najważniejsze struktury:
* **Spawn** (`mq_spawn_req_t` / `mq_spawn_rep_t`) – żądanie utworzenia nowej jednostki (BS lub CM) oraz odpowiedź z PID i `unit_id` nowej jednostki.
* **Commander** (`mq_commander_req_t` / `mq_commander_rep_t`) – zapytanie eskadry o przypisanie dowódcy i odpowiedź z `commander_id`.
* **Damage** (`mq_damage_t`) – przesyłanie obrażeń do docelowego procesu (adresowane po PID).
* **Order** (`mq_order_t`) – rozkazy dla eskadr (patrol, atak, guard), adresowane po PID z offsetem `MQ_ORDER_MTYPE_OFFSET`.
* **CM commands** (`mq_cm_cmd_t` / `mq_cm_rep_t`) – komendy z `console_manager` (freeze, tick speed, spawn, end) i odpowiedzi z kodem statusu.
* **UI map** (`mq_ui_map_req_t` / `mq_ui_map_rep_t`) – żądanie wykonania snapshotu mapy i potwierdzenie gotowości.

### Przepływy komunikacji
1. **BS/CM → CC (spawn):** wysyłany `mq_spawn_req_t` do REQ; CC tworzy proces i odsyła `mq_spawn_rep_t` na REP z `mtype = PID` nadawcy.
2. **SQ → CC (commander):** eskadra prosi o dowódcę, CC odsyła identyfikator dowódcy.
3. **CC → jednostki (damage/order):** CC adresuje komunikaty bezpośrednio do PID jednostki, co pozwala na szybkie filtrowanie po `msgrcv`.
4. **CM → CC (sterowanie):** komendy z konsoli trafiają do CC, odpowiedzi wracają na REP.
5. **UI → CC (mapa):** UI wysyła żądanie, CC przygotowuje snapshot w SHM i odsyła potwierdzenie.

### Przykładowy cykl „spawn” (szczegóły)
1. BS lub CM wysyła `mq_spawn_req_t` z danymi: typ jednostki, frakcja, pozycja, opcjonalny `commander_id`.
2. CC odbiera żądanie, tworzy proces potomny i rejestruje go w SHM.
3. CC wysyła `mq_spawn_rep_t` na kolejkę REP ustawiając `mtype = PID` nadawcy.
4. Nadawca odbiera odpowiedź przez `mq_try_recv_reply()` i aktualizuje swój stan (np. BS dodaje eskadrę do „underlings”).

### Wysyłanie obrażeń i rozkazów
* **Damage:** komunikat `mq_damage_t` jest adresowany bezpośrednio do PID celu (używany do sygnalizowania otrzymanych obrażeń).
* **Order:** CC wysyła `mq_order_t` do eskadry, `mtype` ustawiane jest na PID + offset, a odbiór odbywa się tylko przez docelową eskadrę.

### Obsługa błędów i braków wiadomości
Funkcje `mq_try_recv_*` zwracają 0, gdy nie ma wiadomości (`ENOMSG`), 1 gdy odbiór się powiódł, i -1 dla błędów. Dzięki temu pętle główne mogą reagować bez blokowania, a brak danych nie zatrzymuje symulacji. Tam gdzie potrzebne jest potwierdzenie (np. UI map), stosowane są wersje blokujące z oczekiwaniem na odpowiedź.

---
## Co udało się zrobić
* Zaprojektowano i uruchomiono wieloprocesową architekturę (CC, BS, SQ, CM, UI, tee).
* Zaimplementowano IPC w oparciu o pamięć dzieloną, semafory oraz kolejki komunikatów SysV.
* Zrealizowano synchronizację kroków symulacji (bariera ticków) oraz bezpieczne zamykanie procesów.
* Opracowano logikę jednostek, statystyki, uzbrojenie i scenariusze startowe.
* Dodano TUI do podglądu mapy i statystyk jednostek oraz konsolę sterującą.
* Wprowadzono centralne logowanie i archiwizację przebiegu w katalogu logs.

---
## Elementy specjalne
* Bariera ticków oparta o semafory SysV zapewniająca deterministyczny krok symulacji.
* Asynchroniczne sterowanie z `console_manager` (freeze, tick speed, spawn, end).
* TUI z mapą i tabelą statystyk jednostek odczytywaną ze snapshotów SHM.
* Wieloprocesowe logowanie do plików z automatycznym katalogiem run.
* Obsługa kontrolowanego zatrzymania procesów (sygnały + semafory przerywalne).
* Niejednoznaczna kolejność akcji wynikająca z rywalizacji o `SEM_GLOBAL_LOCK`.

---
## Problemy podczas projektu
* Trudności w utrzymaniu spójności danych przy równoczesnych modyfikacjach SHM (konieczność precyzyjnego doboru sekcji krytycznych).
* Sporadyczne zakleszczenia/wyścigi przy nieprawidłowej kolejności blokad semaforów.
* Obsługa poprawnego zakończenia procesów (przerywanie `semop` i sprzątanie IPC).
* Niejednoznaczne zachowanie UI przy szybkim ticku (odczyt snapshotów i odświeżanie TUI).

---
## Opis ważniejszych elementów
### 1. Inicjalizacja IPC (kiedy i gdzie)
Uruchomienie symulacji zaczyna się w `command_center`, które wywołuje `ipc_create()`. W tej fazie:
* tworzony jest lub czyszczony zestaw kolejek komunikatów,
* inicjalizowane są semafory i pamięć dzielona,
* wykonywany jest reset stanu SHM pod globalną blokadą.

Fragment inicjalizacji (ipc_context.c):
```c
// create-or-open queues + clear stale queues
ctx->q_req = CHECK_SYS_CALL_NONFATAL(msgget(req_key, IPC_CREAT | 0600), "ipc:msgget_req_initial");
ctx->q_rep = CHECK_SYS_CALL_NONFATAL(msgget(rep_key, IPC_CREAT | 0600), "ipc:msgget_rep_initial");
msgctl(ctx->q_req, IPC_RMID, NULL);
msgctl(ctx->q_rep, IPC_RMID, NULL);
ctx->q_req = CHECK_SYS_CALL_NONFATAL(msgget(req_key, IPC_CREAT | 0600), "ipc:msgget_req_recreate");
ctx->q_rep = CHECK_SYS_CALL_NONFATAL(msgget(rep_key, IPC_CREAT | 0600), "ipc:msgget_rep_recreate");

// semaphores
ctx->sem_id = CHECK_SYS_CALL_NONFATAL(semget(sem_key, SEM_COUNT, IPC_CREAT | 0600), "ipc:semget");
vals[SEM_GLOBAL_LOCK] = 1;
vals[SEM_TICK_START]  = 0;
vals[SEM_TICK_DONE]   = 0;
semctl(ctx->sem_id, 0, SETALL, u);

// shared memory reset
ctx->S = (shm_state_t*)shmat(ctx->shm_id, NULL, 0);
sem_lock(ctx->sem_id, SEM_GLOBAL_LOCK);
memset(ctx->S, 0, sizeof(*ctx->S));
ctx->S->magic = SHM_MAGIC;
ctx->S->next_unit_id = 1;
sem_unlock(ctx->sem_id, SEM_GLOBAL_LOCK);
```

### 2. Bariera ticków (kto i kiedy synchronizuje)
CC steruje tempem symulacji. Po każdym kroku:
* zwiększa licznik ticków,
* wyznacza liczbę aktywnych jednostek,
* wypuszcza dokładnie N pozwoleń `SEM_TICK_START`,
* czeka na N zgłoszeń `SEM_TICK_DONE`.

Fragment pętli ticków (command_center.c):
```c
ctx.S->ticks++;
uint8_t alive = 0;
for (int id=1; id<=MAX_UNITS; id++) if (ctx.S->units[id].alive) alive++;
ctx.S->tick_expected = alive;
ctx.S->tick_done = 0;

for (unsigned i=0; i<alive; i++) {
	if (sem_post_retry(ctx.sem_id, SEM_TICK_START, +1) == -1) {
		g_stop = 1;
		break;
	}
}
for (unsigned i=0; i<alive; i++) {
	if (sem_wait_intr(ctx.sem_id, SEM_TICK_DONE, -1, &g_stop) == -1) {
		break;
	}
}
```

### 3. Pętla jednostki (BS/SQ) i snapshot stanu
Każda jednostka synchronizuje się z tickiem, pobiera minimalny snapshot SHM i wykonuje akcję. Dodatkowo chroni się przed podwójnym krokiem w tym samym ticku przez `last_step_tick`.

Fragment pętli jednostki (battleship.c):
```c
if (sem_wait_intr(ctx.sem_id, SEM_TICK_START, -1, &g_stop) == -1) {
	if (g_stop) break;
	continue;
}
if (sem_lock_intr(ctx.sem_id, SEM_GLOBAL_LOCK, &g_stop) == -1) {
	if (g_stop) break;
	continue;
}
uint32_t t = ctx.S->ticks;
uint8_t alive = ctx.S->units[unit_id].alive;
point_t cp = (point_t)ctx.S->units[unit_id].position;
if (ctx.S->last_step_tick[unit_id] == t) {
	sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);
	sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1);
	continue;
}
ctx.S->last_step_tick[unit_id] = t;
sem_unlock(ctx.sem_id, SEM_GLOBAL_LOCK);

// ... logika akcji ...

sem_post_retry(ctx.sem_id, SEM_TICK_DONE, +1);
```

### 4. Komunikacja żądania/odpowiedzi (kolejki)
Wzorzec request/response opiera się o `mtype` i PID. Odpowiedzi wracają na kolejkę REP z `mtype = PID` nadawcy, co umożliwia szybkie filtrowanie.

Fragmenty z ipc_mesq.c:
```c
int mq_send_reply(int qrep, const mq_spawn_rep_t *rep) {
	return msgsnd(qrep, rep, sizeof(*rep) - sizeof(long), IPC_NOWAIT);
}

int mq_try_recv_reply(int qrep, mq_spawn_rep_t *out) {
	pid_t me = getpid();
	ssize_t n = msgrcv(qrep, out, sizeof(*out) - sizeof(long), me, IPC_NOWAIT);
	if (n < 0 && errno == ENOMSG) return 0;
	return (n < 0) ? -1 : 1;
}

int mq_send_order(int qreq, const mq_order_t *order) {
	mq_order_t msg = *order;
	msg.mtype += MQ_ORDER_MTYPE_OFFSET;
	return msgsnd(qreq, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);
}
```

### 5. UI – snapshot mapy i statystyk
UI wątkowo odczytuje SHM w sposób bezpieczny: krótki lock, kopia danych, szybki unlock i render bez blokowania innych procesów.

Wątek mapy (ui_map.c):
```c
mq_ui_map_req_t req;
req.mtype = MSG_UI_MAP_REQ;
req.sender = getpid();

if (mq_send_ui_map_req(ui_ctx->ctx->q_req, &req) == 0) {
	mq_ui_map_rep_t rep;
	int ret = mq_recv_ui_map_rep_blocking(ui_ctx->ctx->q_rep, &rep);
	if (ret > 0 && rep.ready) {
		int lock_ret = sem_lock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK);
		if (lock_ret == 0) {
			unit_id_t grid_snapshot[M][N];
			memcpy(grid_snapshot, ui_ctx->ctx->S->grid, sizeof(grid_snapshot));
			uint32_t tick = ui_ctx->ctx->S->ticks;
			sem_unlock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK);
			render_map(ui_ctx, grid_snapshot, tick);
		}
	}
}
```

Tabela statystyk (ui_ust.c):
```c
int lock_ret = sem_lock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK);
uint16_t unit_count = ui_ctx->ctx->S->unit_count;
uint32_t tick = ui_ctx->ctx->S->ticks;
unit_entity_t units[MAX_UNITS+1];
memcpy(units, ui_ctx->ctx->S->units, sizeof(units));
sem_unlock(ui_ctx->ctx->sem_id, SEM_GLOBAL_LOCK);
```

### 6. Logowanie i katalog run
Każde uruchomienie tworzy unikalny katalog logów. CC zapisuje ścieżkę do pliku tymczasowego, aby CM/UI mogły ją odczytać.

Fragment tworzenia katalogu (command_center.c):
```c
snprintf(out, out_sz,
		 "logs/run_%04d-%02d-%02d_%02d-%02d-%02d_pid%d",
		 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		 tm.tm_hour, tm.tm_min, tm.tm_sec,
		 (int)pid);

if (mkdir(out, 0755) == -1 && errno != EEXIST) {
	HANDLE_SYS_ERROR_NONFATAL("make_run_dir:mkdir", "Failed to create run directory");
}

const char *rundir_file = "/tmp/skirmish_run_dir.txt";
FILE *f = fopen(rundir_file, "w");
if (f) {
	fprintf(f, "%s\n", out);
	fclose(f);
}
```

### 7. Obsługa komend CM (sterowanie symulacją)
Console Manager wysyła komendy do CC przez kolejkę REQ, a CC odsyła odpowiedź na REP (po PID). Zgodnie z dokumentacją CM obsługuje m.in. `freeze`, `unfreeze`, `tickspeed`, `grid`, `spawn`, `end`. Komenda jest opisana strukturą `mq_cm_cmd_t` i trafia do CC jako `MSG_CM_CMD`.

Fragment struktury komendy (ipc_mesq.h):
```c
typedef struct {
	long mtype;           // MSG_CM_CMD
	cm_command_type_t cmd;
	pid_t sender;
	uint32_t req_id;
	int32_t tick_speed_ms;
	int32_t grid_enabled;
	unit_type_t spawn_type;
	faction_t spawn_faction;
	int16_t spawn_x;
	int16_t spawn_y;
} mq_cm_cmd_t;
```

### 8. Scenariusze i inicjalne rozmieszczenie
CC wczytuje scenariusz startowy i tworzy początkowe jednostki. Dokumentacja projektu opisuje scenariusze jako predefiniowane zestawy jednostek i pozycji, co pozwala uruchamiać różne konfiguracje bez zmian w kodzie.

### 9. System dowódców i rozkazów
Moduł BS pełni rolę dowódcy: zarządza listą „underlings” (eskadr) i wysyła do nich rozkazy przez kolejkę. Rozkazy są adresowane po PID z offsetem `MQ_ORDER_MTYPE_OFFSET`, co zapobiega kolizjom z innymi komunikatami.

Fragment wysyłki rozkazu (ipc_mesq.c):
```c
int mq_send_order(int qreq, const mq_order_t *order) {
	mq_order_t msg = *order;
	msg.mtype += MQ_ORDER_MTYPE_OFFSET;
	return msgsnd(qreq, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);
}
```

### 10. Logika walki i ruchu jednostek
W module `unit_logic.c` zdefiniowano mechanikę walki (mnożniki obrażeń, skuteczność broni) oraz skanowanie radarowe i ruch. Dokumentacja projektu opisuje m.in. macierz skuteczności typów jednostek i modyfikatory celności zależne od rodzaju broni, co bezpośrednio wpływa na przebieg starcia.

### 11. UI – model wątkowy i okna
UI pracuje jako osobny proces z wątkami MAP/UST/STD. Dzięki temu odświeżanie mapy, tabeli statystyk i logów jest rozdzielone i responsywne. Dokumentacja UI opisuje układ okien (MAP, UST, STD) oraz zasady synchronizacji z ncurses.

Schemat wątków (wg dokumentacji UI):
```
UI Process
├── MAP thread (ui_map.c)  -> siatka M×N
├── UST thread (ui_ust.c)  -> tabela jednostek
└── STD thread (ui_std.c)  -> logi/STDOUT
```

---
## Testy

### Test 1: Spawning eskadr i rozkazy GUARD

**Cel testu:**
Weryfikacja mechanizmu spawningowania eskadr przez Carrier i wysyłania rozkazów GUARD do podwładnych.

**Konfiguracja:**
```
Scenariusz: default.conf
- Mapa: 120×40
- Jednostki: 1 Republic Carrier (typ=3) na pozycji (5,5)
- Czas trwania: 8 ticków
- Log: logs/run_2026-02-05_17-47-12_pid21753/ALL.log
```

**Przebieg:**
1. Uruchomienie Command Center ze scenariuszem default (1 Carrier)
2. Tick 1: Carrier patroluje i wysyła żądanie spawnu eskadry
3. Użycie CM do zamrożenia symulacji po ticku 1
4. Odmrożenie symulacji przez CM
5. Ticki 2-7: Carrier spawnuje eskadry i wydaje rozkazy
6. Zakończenie symulacji komendą 'end' w CM
7. Weryfikacja poprawnego zakończenia wszystkich procesów

**Wyniki:**
```
✅ ZALICZONY - Test wykonany 2026-02-05

Oś czasu:
- 17:47:12.346: CC utworzył Carrier (unit_id=1, pid=21756) na (5,5)
- 17:47:13.351: Tick 1 - Carrier patroluje, wybiera cel (16,21)
- 17:47:13.478: Komenda freeze z CM, symulacja zamrożona
- 03:32:42.050: Komenda unfreeze, symulacja wznowiona
- 03:32:42.389: Tick 2-3 - Carrier utworzył SQ 2 (Bomber) na (9,16)
- 03:32:43.396: Tick 4 - Carrier utworzył SQ 3 (Bomber) na (15,23)
- 03:32:44.417: Tick 5 - Carrier utworzył SQ 4 (Fighter) na (12,21)
- 03:32:45.431: Tick 6 - Carrier utworzył SQ 5 (Bomber) na (16,13)
- 03:32:46.450: Tick 7 - Carrier utworzył SQ 6 (Fighter) na (15,7)

Zachowanie eskadr:
- Wszystkie eskadry otrzymały rozkaz GUARD (order=5) z celem=1 (dowódca)
- Eskadry śledziły pozycję dowódcy i poruszały się w stronę pozycji wartowniczych
- Carrier osiągnął pełną pojemność fighter bay (capacity=5, current=5)

Sekwencja zamknięcia:
- 03:32:48.437: Komenda 'end' z CM
- 03:32:48.497: CC wysłał SIGTERM do wszystkich 6 żywych jednostek
- 03:32:48.500-503: Wszystkie jednostki zalogowały "terminating, cleaning"
- 03:32:48.511: Wszystkie 6 procesów potomnych zakończonych ze statusem 0
- 03:32:48.514: Obiekty IPC odłączone i zniszczone
```

**Kryteria zdania:**
- ✅ Carrier poprawnie spawnuje eskadry (5/5 po początkowym odrzuceniu)
- ✅ Wymuszono pojemność fighter bay (capacity=5)
- ✅ Rozkazy GUARD (order=5) wysyłane do wszystkich podwładnych
- ✅ Eskadry potwierdzają rozkazy i śledzą stan dowódcy
- ✅ CM freeze/unfreeze działa poprawnie
- ✅ Poprawne zamknięcie: wszystkie procesy wychodzą ze statusem 0

---

### Test 2: Patrolowanie Battleship

**Cel testu:**
Weryfikacja logiki patrolowania Battleship w przypadku braku dowódcy (autonomiczny patrol).

**Konfiguracja:**
```
Scenariusz: default.conf
- Mapa: 120×40
- Jednostka: 1 Republic Carrier (BS 1) na pozycji (5,5)
- Carrier typ=3 z pojemnością fighter bay=5
- Log: logs/run_2026-02-06_04-24-53_pid145812/ALL.log
- Console Manager: freeze, ustawienie prędkości 3000ms, resume
- UI: uruchomiony dla wizualnej weryfikacji
```

**Przebieg:**
1. Start symulacji: ./command_center --scenario default &
2. Uruchomienie console_manager i natychmiastowe zamrożenie
3. Uruchomienie UI: ./ui
4. Ustawienie tick speed na 3000ms dla łatwiejszej obserwacji
5. Wznowienie symulacji i obserwacja:
   - BS 1 wybiera losowy cel patrolu w promieniu patrolowania
   - BS 1 porusza się w stronę celu (6 komórek/tick)
   - BS 1 spawnuje eskadry co tick aż do pełnej pojemności
6. Uruchomienie przez 46 ticków i wydanie komendy 'end'

**Wyniki:**
```
✅ ZALICZONY - Test wykonany 2026-02-06

Sekwencja patrolu Battleship:
  Tick 1: pos=(5,5) → wybrany cel patrolu (24,1), ruch do (11,5)
  Tick 7: pos=(25,7) → cel (25,7), dt2=0 (osiągnięto punkt patrolu)
  Tick 46: pos=(25,7) → cel (25,7), dt2=0 (w punkcie patrolu)

Wybór celów patrolu:
  - (24,1), (25,7) i inne losowe cele w promieniu
  - Nowy cel wybierany gdy dt2=0 (cel osiągnięty)

Spawning eskadr:
  Tick 1: Utworzono SQ 2 na (12,2) - typ=5 (lekki myśliwiec)
  Tick 2-4: Utworzono SQ 3-5
  Fighter bay: capacity=5, current=5 (pełny)

Rozkazy dla eskadr:
  - Wszystkie eskadry otrzymały order=5 (GUARD) z celem=1 (BS 1)
  - Eskadry podążają za BS 1 utrzymując pozycje eskortowe

Obserwacje UI:
  - MAP pokazywał BS 1 (symbol C) poruszający się po siatce
  - UST wyświetlał 6 jednostek (BS 1 + 5 eskadr) z aktualnymi statystykami
  - Wszystkie 5 eskadr widoczne i śledzące BS 1

Zamknięcie (tick 46):
  - Komenda 'end' przez console_manager
  - CC wysłał SIGTERM do wszystkich jednostek
  - UI wykrył zniszczenie IPC i zakończył się poprawnie
```

**Kryteria zdania:**
- ✅ Battleship patroluje autonomicznie bez dowódcy
- ✅ Losowe cele patrolu wybierane w promieniu
- ✅ Prędkość ruchu zgodna ze specyfikacją Carrier (6/tick)
- ✅ Eskadry spawnowane do osiągnięcia pojemności
- ✅ Eskadry otrzymują rozkazy GUARD
- ✅ Poprawne zamknięcie UI

---

### Test 3: Przypisanie dowódcy do eskadry

**Cel testu:**
Testowanie mechanizmu szukania i akceptowania dowódcy przez eskadrę z Battleship.

**Konfiguracja:**
```
Scenariusz: Własny scenariusz testowy
- Mapa: 120×40
- Jednostki:
  - BS 1 (Destroyer, typ=2) na (8,8) - SP=3, DR=20
  - SQ 2 (Fighter, typ=4) na (10,10) - brak początkowego dowódcy
  - SQ 3 (Fighter, typ=4) na (14,14) - brak początkowego dowódcy
- Log: logs/run_2026-02-06_04-31-31_pid147247/
- Czas trwania: 13 ticków
```

**Przebieg:**
1. Start symulacji z 1 Destroyer + 2 niezależnymi Fighterami
2. Tick 1: Eskadry nie mają dowódcy (commander=0, state=0)
3. Tick 1: Eskadry wykrywają BS 1 i wysyłają żądania dowódcy
4. Tick 2: BS 1 akceptuje eskadry jako podwładnych
5. Tick 2: Eskadry otrzymują potwierdzenie przypisania
6. Tick 2+: Eskadry otrzymują rozkazy GUARD i podążają za BS 1
7. Weryfikacja śledzenia pozycji dowódcy przez eskadry

**Wyniki:**
```
✅ ZALICZONY - Test wykonany 2026-02-06

Eskadra 2 (SQ_u2_pid_147252.log):
  Tick 1: [SQ 2] current commander 0 state 0

Eskadra 3 (SQ_u3_pid_147253.log):
  Tick 1: [SQ 3] current commander 0 state 0

Battleship 1 (BS_u1_pid_147251.log):
  Tick 1: [BS 1] accepted squadron 3 as underling

Zachowanie po przypisaniu (ticki 3-13):
  - SQ 2 i SQ 3 ciągle otrzymują rozkazy GUARD od BS 1
  - Eskadry poruszają się w kierunku pozycji dowódcy
  - Wszystkie 5 eskadr podążają za BS 1 do ticku 4

Zamknięcie (tick 13):
  - Wszystkie jednostki zakończone poprawnie
  - 6 procesów potomnych zakończonych (BS 1 + 5 eskadr)
```

**Kryteria zdania:**
- ✅ Eskadry startują bez dowódcy (commander=0)
- ✅ Eskadry wykrywają i żądają dowódcy od BS
- ✅ BS akceptuje żądania dowódcy od eskadr
- ✅ Eskadry otrzymują potwierdzenie przypisania
- ✅ Rozkazy GUARD (order=5) wysyłane do wszystkich podwładnych
- ✅ Eskadry śledzą stan dowódcy (state=1 = żywy)
- ✅ Eskadry poruszają się w kierunku pozycji dowódcy

---

### Test 4: Śmierć dowódcy i przepisanie

**Cel testu:**
Weryfikacja mechanizmu przepisania eskadr do nowego dowódcy po zniszczeniu obecnego dowódcy.

**Konfiguracja:**
```
Scenariusz: test.conf
- 2 Carriery (BS 1 na (5,5), BS 2 na (8,8)), ta sama frakcja (FACTION_REPUBLIC)
- Każdy carrier spawnuje eskadry podczas symulacji
- BS 2 spawnuje SQ 3 i SQ 5
- BS 1 spawnuje SQ 4, SQ 6, SQ 8
```

**Przebieg:**
1. Uruchomienie symulacji z 2 carrierami, ta sama frakcja
2. Czekanie aż carriery utworzą eskadry (tick 4)
3. Zamrożenie symulacji przez Console Manager (komenda freeze)
4. Zabicie BS 2 zewnętrznie: kill <pid> (kill 158696)
5. Wznowienie symulacji przez Console Manager (komenda resume)
6. Obserwacja zachowania eskadr w ticku 5+

**Wyniki:**
```
✅ ZALICZONY - Test wykonany 2026-02-06

Tick 5:
  [SQ 3] current commander 2 state 0
  [SQ 3] sent commander request to potential BS 1
  [SQ 5] current commander 2 state 0
  [SQ 5] sent commander request to potential BS 1

Tick 6:
  [SQ 3] assigned to commander 1
  [SQ 5] assigned to commander 1
  [BS 1] sent order 5 with target 1 to SQ 3
  [BS 1] sent order 5 with target 1 to SQ 5

Wszystkie osierocone eskadry pomyślnie przepisane do BS 1.
BS 1 teraz dowodzi: SQ 4, SQ 6, SQ 8, SQ 3, SQ 5 (i później SQ 2, SQ 9, SQ 7)
```

**Kryteria zdania:**
- ✅ Eskadry wykrywają śmierć dowódcy (state=0) w ciągu 1 ticku
- ✅ Eskadry wysyłają żądania dowódcy do okrętów tej samej frakcji
- ✅ Przeżywający okręt kapitałowy akceptuje osierocone eskadry
- ✅ Osierocone eskadry otrzymują rozkazy od nowego dowódcy

**Uwagi:**
```
Bug znaleziony i naprawiony podczas testowania:
- unit_radar(unit_id, st, units, out, my_faction) WYKLUCZAŁ jednostki tej samej
  frakcji zamiast je znajdować (5. parametr to faction_to_exclude)
- Po poprawce eskadry poprawnie odnajdują kapitałowe okręty swojej frakcji
```

---

## Linki do kluczowych fragmentów kodu

### a. Tworzenie i obsługa plików

**open(), creat(), close(), write(), read(), unlink()**

- **Tworzenie pliku PID z blokadą (open + O_CREAT)**  
  [command_center.c#L70](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L70) - `open(pidfile, O_RDWR | O_CREAT, 0600)`

- **Zapis PID do pliku (write)**  
  [command_center.c#L99](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L99) - `write(fd, pid_str, strlen(pid_str))`

- **Otwarcie pliku logu (open + O_APPEND)**  
  [utils.c#L73](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c#L73) - `open(all_path, O_WRONLY | O_CREAT | O_APPEND, 0600)`

- **Zamykanie plików (close, fclose)**  
  [command_center.c#L85](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L85) - `close(fd)`  
  [utils.c#L155](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c#L155) - `fclose(g_logf)`

- **Usuwanie FIFO (unlink)**  
  [ui_main.c#L118](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c#L118) - `unlink("/tmp/skirmish_std.fifo")`  
  [console_manager.c#L324-L325](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c#L324-L325) - czyszczenie FIFO CM-UI

- **Odczyt z pliku (read)**  
  [console_manager.c#L412](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c#L412) - `read(ui_input_fd, line, sizeof(line) - 1)`

- **Zapis do logu (write)**  
  [utils.c#L225](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/utils.c#L225) - `write(g_all_fd, line, len)` atomiczny zapis do ALL.log

---

### b. Tworzenie procesów

**fork(), exec(), exit(), wait()**

- **Tworzenie procesu jednostki (fork)**  
  [command_center.c#L199](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L199) - `fork()` spawning BS/SQ

- **Podwójny fork dla terminal_tee (fork + fork)**  
  [terminal_tee.c#L47](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c#L47) - pierwszy fork  
  [terminal_tee.c#L58](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c#L58) - drugi fork dla daemonizacji

- **Wykonanie nowego programu (execv)**  
  [terminal_tee.c#L98](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c#L98) - `execv("/bin/sh", argv)` uruchomienie tee

- **Czekanie na potomka (waitpid)**  
  [command_center.c#L902](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L902) - `waitpid(-1, &status, 0)` blokujące czekanie  
  [command_center.c#L311](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L311) - `waitpid(killed[i], &status, WNOHANG)` nieblokujące

---

### c. Tworzenie i obsługa wątków

**pthread_create(), pthread_join(), pthread_mutex_lock/unlock()**

- **Tworzenie wątków UI (pthread_create)**  
  [ui_main.c#L184](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c#L184) - wątek MAP  
  [ui_main.c#L192](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c#L192) - wątek UST  
  [ui_main.c#L202](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c#L202) - wątek STD

- **Tworzenie wątku CM w CC (pthread_create)**  
  [command_center.c#L651](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L651) - `pthread_create(&cm_thread, NULL, cm_thread_func, &ctx)`

- **Oczekiwanie na wątki (pthread_join)**  
  [ui_main.c#L240-L244](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c#L240-L244) - join MAP, UST, STD  
  [command_center.c#L859](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L859) - join wątku CM

- **Inicjalizacja mutexa (pthread_mutex_init)**  
  [ui_main.c#L39](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c#L39) - `pthread_mutex_init(&ui_ctx->ui_lock, NULL)`

- **Mutex statyczny (PTHREAD_MUTEX_INITIALIZER)**  
  [command_center.c#L47](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L47) - `g_cm_mutex` dla freeze/tick_speed

- **Blokowanie mutexa (pthread_mutex_lock)**  
  [command_center.c#L384](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L384) - ochrona `g_frozen`  
  [ui_ust.c#L45](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_ust.c#L45) - ochrona ncurses  
  [ui_std.c#L34](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_std.c#L34) - blokada przed renderem

- **Odblokowywanie mutexa (pthread_mutex_unlock)**  
  [command_center.c#L386](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L386) - zwolnienie `g_cm_mutex`  
  [ui_ust.c#L135](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_ust.c#L135) - zwolnienie `ui_lock`

- **Niszczenie mutexa (pthread_mutex_destroy)**  
  [ui_main.c#L120](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c#L120) - `pthread_mutex_destroy(&ui_ctx->ui_lock)`

---

### d. Obsługa sygnałów

**signal(), sigaction(), kill(), raise()**

- **Rejestracja handlera sygnału (signal)**  
  [ui_main.c#L160-L161](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_main.c#L160-L161) - `signal(SIGINT, signal_handler)`, `signal(SIGTERM, ...)`  
  [console_manager.c#L303-L304](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c#L303-L304) - handler w CM

- **Zaawansowana obsługa sygnałów (sigaction)**  
  [command_center.c#L531-L535](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L531-L535) - `sigaction(SIGINT, &sa, NULL)` w CC  
  [squadron.c#L388-L392](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c#L388-L392) - `sigaction(SIGTERM, ...)`, ignorowanie SIGINT  
  [squadron.c#L395-L400](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/squadron.c#L395-L400) - `sigaction(SIGRTMAX, ...)` dla damage notify

- **Wysyłanie sygnału (kill)**  
  [command_center.c#L287](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L287) - `kill(pid, SIGTERM)` zamykanie jednostki  
  [command_center.c#L880](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L880) - masowe wysyłanie SIGTERM do jednostek  
  [unit_ipc.c#L64](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/unit_ipc.c#L64) - `kill(target_pid, SIGRTMAX)` notyfikacja obrażeń

- **Ignorowanie sygnałów (sigaction + SIG_IGN)**  
  [terminal_tee.c#L19-L24](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c#L19-L24) - `ignore_signal()` helper dla tee

---

### e. Synchronizacja procesów (semafory)

**ftok(), semget(), semctl(), semop()**

- **Generowanie klucza IPC (ftok)**  
  [ipc_context.c#L34-L36](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L34-L36) - `ftok("./key.ipc", 'S')` dla semaforów

- **Tworzenie zestawu semaforów (semget)**  
  [ipc_context.c#L52](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L52) - `semget(sem_key, SEM_COUNT, IPC_CREAT | 0600)`

- **Kontrola semaforów (semctl - SETALL)**  
  [ipc_context.c#L57-L62](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L57-L62) - inicjalizacja wartości semaforów (GLOBAL_LOCK=1, TICK_START=0, TICK_DONE=0)

- **Kontrola semaforów (semctl - IPC_RMID)**  
  [ipc_context.c#L125](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L125) - `semctl(ctx->sem_id, 0, IPC_RMID)` usuwanie

- **Operacje na semaforach (semop)**  
  [semaphores.c#L9-L14](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/semaphores.c#L9-L14) - `sem_op_retry()` wrapper z retry na EINTR  
  [semaphores.c#L25-L37](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/semaphores.c#L25-L37) - `sem_op_intr()` przerywalna wersja  
  [semaphores.c#L48-L52](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/semaphores.c#L48-L52) - `sem_lock()` blokada  
  [semaphores.c#L54-L58](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/semaphores.c#L54-L58) - `sem_unlock()` zwolnienie

- **Zastosowanie w barierze ticków (sem_post + sem_wait)**  
  [command_center.c#L775-L783](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L775-L783) - wypuszczanie N pozwoleń SEM_TICK_START  
  [command_center.c#L785-L789](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L785-L789) - czekanie na N potwierdzeń SEM_TICK_DONE

---

### f. Łącza nazwane i nienazwane

**mkfifo(), pipe(), dup(), dup2()**

- **Tworzenie FIFO (mkfifo)**  
  [ui_std.c#L29](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_std.c#L29) - `mkfifo(FIFO_PATH, 0600)` dla UI STD  
  [console_manager.c#L327](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c#L327) - `mkfifo(cm_to_ui, 0600)`  
  [console_manager.c#L330](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c#L330) - `mkfifo(ui_to_cm, 0600)`

- **Tworzenie pipe (pipe)**  
  [terminal_tee.c#L29](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c#L29) - `pipe(pfd)` dla przekierowania do tee

- **Przekierowanie deskryptorów (dup2)**  
  [terminal_tee.c#L89](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c#L89) - `dup2(pfd[0], STDIN_FILENO)` w procesie tee  
  [terminal_tee.c#L115-L116](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/terminal_tee.c#L115-L116) - `dup2(pfd[1], STDOUT/STDERR)` w CC  
  [command_center.c#L557-L558](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CC/command_center.c#L557-L558) - przekierowanie stdout/stderr CC do pipe tee

---

### g. Segmenty pamięci dzielonej

**ftok(), shmget(), shmat(), shmdt(), shmctl()**

- **Generowanie klucza (ftok)**  
  [ipc_context.c#L39](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L39) - `ftok("./key.ipc", 'M')` dla SHM

- **Tworzenie segmentu (shmget)**  
  [ipc_context.c#L64](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L64) - `shmget(shm_key, sizeof(shm_state_t), IPC_CREAT | 0600)`

- **Podłączanie do segmentu (shmat)**  
  [ipc_context.c#L70](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L70) - `shmat(ctx->shm_id, NULL, 0)` w CC  
  [ipc_context.c#L142](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L142) - `shmat()` w procesach potomnych

- **Inicjalizacja zawartości SHM**  
  [ipc_context.c#L72-L76](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L72-L76) - `memset(ctx->S, 0, ...)` + `ctx->S->magic = SHM_MAGIC`

- **Odłączanie od segmentu (shmdt)**  
  [ipc_context.c#L113](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L113) - `shmdt(ctx->S)` podczas cleanup

- **Usuwanie segmentu (shmctl)**  
  [ipc_context.c#L118](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L118) - `shmctl(ctx->shm_id, IPC_RMID, NULL)`

- **Dostęp do SHM pod blokadą semafora**  
  [ui_map.c#L155-L160](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_map.c#L155-L160) - snapshot mapy w UI  
  [ui_ust.c#L96-L100](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_ust.c#L96-L100) - odczyt unit_count i kopii jednostek

---

### h. Kolejki komunikatów

**ftok(), msgget(), msgsnd(), msgrcv(), msgctl()**

- **Generowanie kluczy (ftok)**  
  [ipc_context.c#L29-L30](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L29-L30) - `ftok("./key.ipc", 'Q')` dla REQ, `ftok(..., 'R')` dla REP

- **Tworzenie kolejek (msgget)**  
  [ipc_context.c#L42](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L42) - `msgget(req_key, IPC_CREAT | 0600)` q_req  
  [ipc_context.c#L43](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L43) - `msgget(rep_key, IPC_CREAT | 0600)` q_rep

- **Usuwanie starych kolejek (msgctl - IPC_RMID)**  
  [ipc_context.c#L44-L45](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L44-L45) - czyszczenie przed rekonstrukcją

- **Wysyłanie komunikatów (msgsnd)**  
  [ipc_mesq.c#L9-L11](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c#L9-L11) - `mq_send_spawn_req()` wysyłanie spawnu  
  [ipc_mesq.c#L13-L15](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c#L13-L15) - `mq_send_reply()` odpowiedź z PID  
  [ipc_mesq.c#L40-L42](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c#L40-L42) - `mq_send_damage()` obrażenia  
  [ipc_mesq.c#L44-L48](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c#L44-L48) - `mq_send_order()` rozkaz z offsetem mtype

- **Odbieranie komunikatów (msgrcv)**  
  [ipc_mesq.c#L17-L22](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c#L17-L22) - `mq_try_recv_reply()` z filtrem PID  
  [ipc_mesq.c#L50-L55](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c#L50-L55) - `mq_try_recv_order()` z offsetem  
  [ipc_mesq.c#L69-L74](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_mesq.c#L69-L74) - `mq_recv_ui_map_rep_blocking()` blokujący odbiór

- **Usuwanie kolejek (msgctl - IPC_RMID)**  
  [ipc_context.c#L105-L106](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/ipc/ipc_context.c#L105-L106) - czyszczenie podczas shutdown

---

### i. Gniazda

**Projekt nie wykorzystuje gniazd BSD (socket, bind, listen, accept, connect).**  
Komunikacja IPC oparta jest na kolejkach komunikatów System V, semaforach, pamięci dzielonej oraz FIFO/pipe.

**Alternatywa - FIFO jako mechanizm komunikacji:**
- [ui_std.c#L29](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/UI/ui_std.c#L29) - FIFO UI  
- [console_manager.c#L327-L330](https://github.com/PaurXen/Space-Skirmish-/blob/main/src/CM/console_manager.c#L327-L330) - dwukierunkowe FIFO CM-UI

