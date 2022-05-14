#ifndef LIBCAPYBARA_RECONFIG_H
#define LIBCAPYBARA_RECONFIG_H

#include <stdint.h>
#include <libmsp/mem.h>

#ifdef LIBCAPYBARA_VARTH_ENABLED
#include <libmcppot/mcp4xxx.h>
#endif // LIBCAPYBARA_VARTH_ENABLED

// The following is the lowest-level interface into the reconfigurable power
// system. The units here are bank set and wiper setting, not voltage and
// farads, which are a higher-level concept unnecessary at runtime.

#define CAPYBARA_NUM_BANKS 4
#define CAPYBARA_MAX_THRES POT_RESOLUTION // wiper settings

// Use this to disable the optimization where we only reset the config pins
// if we're switching modes. This is a pure software hack, we need to better
// understand why hardware reverts to the default without being powered off for
// a really long time
#define RECONF_OP_OFF swap_status = 0;

// Configuration table needs to be defined somewhere in the app to set the power
// modes for all of the tasks
#define CAPYBARA_CFG_TABLE(N) \
capybara_task_cfg_t pwr_configs[N]
// Macro for defining one row in the configuration table
#define CFG_ROW(ind,type,opcfg,precfg) {ind,type,pwr_levels + opcfg, pwr_levels+ precfg}


    //{.ind = ind, .type = type, .opcfg =  opcfg, .precfg = precfg}
// Bitmask identifying a set of capacitor banks
typedef uint16_t capybara_bankmask_t;

// Bits reporting status of a precharge operation
typedef uint8_t prechg_status_t;

extern prechg_status_t prechg_status;

// Bits reporting status of a burst operation
typedef uint8_t burst_status_t;

// Bits reporting whether we've successfully swapped configs
typedef uint8_t swap_status_t;

extern burst_status_t burst_status;
extern swap_status_t swap_status;

// Tuple of params that define the pwr system configuration
typedef struct {
    capybara_bankmask_t banks;
#ifdef LIBCAPYBARA_VARTH_ENABLED
    uint16_t vth;
#endif // LIBCAPYBARA_VARTH_ENABLED
} capybara_cfg_t;

extern capybara_cfg_t pwr_levels[];

typedef enum {
    DEFAULT,
    CONFIGD,
    BURST,
    PREBURST,
} capybara_cfg_spec_t;

// Tuple of params, including index and type, that define the configuration for
// a given task
typedef struct {
    uint8_t ind;
    capybara_cfg_spec_t type;
    capybara_cfg_t *opcfg;
    capybara_cfg_t *precfg;
} capybara_task_cfg_t;


#define PWR_LEVEL_TABLE \
X(LOW, 0x0, 2.5) \
X(MEDLOW, 0x4, 2.5) \
X(MED, 0xC, 2.5) \
X(MEDHIGH, 0xA, 2.5) \
X(HIGH, 0xD, 2.5) \
X(LOWP,    0x0, 2.5) \
X(LOWP2,   0x2, 2.5) \
X(MEDLOWP, 0x1, 2.5) \
X(MEDLOWP2, 0x3, 2.5) \
X(MEDP ,   0x4, 2.5) \
X(MEDP2,   0x5, 2.5) \
X(MEDHIGHP,0x7, 2.5) \
X(HIGHP2,   0xD, 2.5) \
X(HIGHP,   0xF, 2.5) \
X(BACKP,   0xC, 2.5) \
X(BACK3P,  0xE, 2.5) \
X(NA,      0x0, 2.5) \

#define X(a, b, c) a,
typedef enum {
    PWR_LEVEL_TABLE
    #undef X
} capybara_pwr_level_t;

extern capybara_task_cfg_t pwr_configs[];
extern capybara_cfg_t base_config;
extern capybara_cfg_t prechg_config;
// Configure the power system runtime params
int capybara_config(capybara_cfg_t cfg);

// Enable the capacitor banks specified in the bitmask
int capybara_config_banks(capybara_bankmask_t banks);

#ifdef LIBCAPYBARA_VARTH_ENABLED
// Set threshold voltage up to which to charge capacitors (units: wiper setting)
int capybara_config_threshold(uint16_t wiper);
#endif // LIBCAPYBARA_VARTH_ENABLED

// Configure settings that store maximum energy
int capybara_config_max();

// Set methods for the base power config
int set_base_banks(capybara_bankmask_t in );
int set_prechg_banks(capybara_bankmask_t in );

// Get & set methods for the precharge status
prechg_status_t get_prechg_status(void);
int set_prechg_status(prechg_status_t);

// Get & set methods for the burst status
burst_status_t get_burst_status(void);
int set_burst_status(burst_status_t);

// Set the precharge status info so on next transition or power on, the
// precharge happens
int issue_precharge(capybara_bankmask_t cfg);

// Function to change the capybara power system configuration after
// transitioning to a new task
void capybara_transition(int);

#endif // LIBCAPYBARA_RECONFIG_H
