// =====================================================================================
//  flash.ino  -  keep Settings in flash between power-ups
// =====================================================================================
//
//  The SAMD21 has no EEPROM, so this saves into the last row of its program flash. The
//  sketch is a small fraction of the 256 KB, so its code never reaches that row.
//
//  Flash rules (SAMD21 NVM controller):
//    - Erase works a row (256 bytes) at a time, and sets every bit to 1.
//    - Writes go into a page buffer first, then a command writes the page (64 bytes).
//    - Each row survives about 25,000 erases, so save only when something changed.
//
//  The saved record, one page:
//
//      [ magic ][ Settings ... ][ check ]
//
//      magic   SAVE_MAGIC: says the row holds this sketch's Settings (and version)
//      check   from the words before it: catches a save cut short by a power loss
//
// =====================================================================================

const uint32_t FLASH_BYTES = 256 * 1024;                // SAMD21G18
const uint32_t ROW_BYTES   = 256;                       // 4 pages of 64 bytes
const uint32_t PAGE_BYTES  = 64;
const uint32_t SAVE_ADDR   = FLASH_BYTES - ROW_BYTES;   // start of the last row

const uint32_t SAVE_MAGIC  = 0x42454E44 + SETTINGS_VERSION;   // "BEND" + version

const int RECORD_WORDS = 1 + sizeof(Settings) / 4 + 1;   // magic + Settings + check
static_assert(RECORD_WORDS * 4 <= PAGE_BYTES, "the saved record must fit in one page");

// -------------------------------------------------------------------------------------
//  loadSettings(s)  -  call once in setup(); false if nothing valid is saved
// -------------------------------------------------------------------------------------
//  Leaves `s` alone unless the record checks out.
// -------------------------------------------------------------------------------------
bool loadSettings(Settings &s) {
  uint32_t record[RECORD_WORDS];
  const volatile uint32_t *flash = (const volatile uint32_t *)SAVE_ADDR;
  for (int i = 0; i < RECORD_WORDS; i++) record[i] = flash[i];

  if (record[0] != SAVE_MAGIC)                         return false;   // blank or old
  if (record[RECORD_WORDS - 1] != recordCheck(record)) return false;   // damaged

  memcpy(&s, &record[1], sizeof(Settings));
  return true;
}

// -------------------------------------------------------------------------------------
//  saveSettings(s)  -  erase the row and write `s` into it
// -------------------------------------------------------------------------------------
//  Takes a few ms, during which the code pauses. Call it only when `s` changed.
// -------------------------------------------------------------------------------------
void saveSettings(const Settings &s) {
  uint32_t record[RECORD_WORDS];
  record[0] = SAVE_MAGIC;
  memcpy(&record[1], &s, sizeof(Settings));
  record[RECORD_WORDS - 1] = recordCheck(record);

  NVMCTRL->CTRLB.bit.MANW = 1;   // write the page only on the WP command below

  // Erase the row. (ADDR counts 16-bit halfwords, hence / 2.)
  NVMCTRL->ADDR.reg = SAVE_ADDR / 2;
  nvmCommand(NVMCTRL_CTRLA_CMD_ER);

  // Fill the page buffer, then write it to the row's first page.
  nvmCommand(NVMCTRL_CTRLA_CMD_PBC);   // clear the page buffer
  volatile uint32_t *flash = (volatile uint32_t *)SAVE_ADDR;
  for (int i = 0; i < RECORD_WORDS; i++) flash[i] = record[i];   // lands in the buffer
  nvmCommand(NVMCTRL_CTRLA_CMD_WP);
}

// -------------------------------------------------------------------------------------
//  recordCheck(record)  -  the check word for a record's magic + Settings
// -------------------------------------------------------------------------------------
//  The sum of those words, inverted, so neither an erased row (all 1s) nor an
//  all-zero one passes.
// -------------------------------------------------------------------------------------
uint32_t recordCheck(const uint32_t *record) {
  uint32_t sum = 0;
  for (int i = 0; i < RECORD_WORDS - 1; i++) sum += record[i];
  return ~sum;
}

// -------------------------------------------------------------------------------------
//  nvmCommand(cmd)  -  run one NVM controller command and wait for it to finish
// -------------------------------------------------------------------------------------
void nvmCommand(uint16_t cmd) {
  NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | cmd;
  while (!NVMCTRL->INTFLAG.bit.READY);
}
