/*
 * CANopen main program file.
 * Modified to send test temperature values via TPDO (OD entry 0x2000).
 */

#include <stdio.h>
#include "CANopen.h"
#include "OD.h"
#include "CO_storageBlank.h"

#define log_printf(macropar_message, ...) printf(macropar_message, ##__VA_ARGS__)

/* default values for CO_CANopenInit() */
#define NMT_CONTROL                                                                                                    \
    CO_NMT_STARTUP_TO_OPERATIONAL                                                                                      \
    | CO_NMT_ERR_ON_ERR_REG | CO_ERR_REG_GENERIC_ERR | CO_ERR_REG_COMMUNICATION
#define FIRST_HB_TIME        500
#define SDO_SRV_TIMEOUT_TIME 1000
#define SDO_CLI_TIMEOUT_TIME 500
#define SDO_CLI_BLOCK        false
#define OD_STATUS_BITS       NULL

/* Global variables and objects */
CO_t* CO = NULL; /* CANopen object */
uint8_t LED_red, LED_green;

/* main ***********************************************************************/
int
main(void) {
    CO_ReturnError_t err;
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    uint32_t heapMemoryUsed;
    void* CANptr = NULL;           /* CAN module address */
    uint8_t pendingNodeId = 10;    /* node ID */
    uint8_t activeNodeId = 10;     
    uint16_t pendingBitRate = 125; /* 125 kbps */

#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
    CO_storage_t storage;
    CO_storage_entry_t storageEntries[] = {{
        .addr = &OD_PERSIST_COMM,
        .len = sizeof(OD_PERSIST_COMM),
        .subIndexOD = 2,
        .attr = CO_storage_cmd | CO_storage_restore,
        .filename = ""
    }};

    uint8_t storageEntriesCount = sizeof(storageEntries) / sizeof(storageEntries[0]);
    uint32_t storageInitError = 0;
#endif

    /* Allocate memory */
    CO_config_t* config_ptr = NULL;
#ifdef CO_MULTIPLE_OD
    CO_config_t co_config = {0};
    OD_INIT_CONFIG(co_config); 
    co_config.CNT_LEDS = 1;
    co_config.CNT_LSS_SLV = 1;
    config_ptr = &co_config;
#endif 
    CO = CO_new(config_ptr, &heapMemoryUsed);
    if (CO == NULL) {
        log_printf("Error: Can't allocate memory\n");
        return 0;
    } else {
        log_printf("Allocated %u bytes for CANopen objects\n", heapMemoryUsed);
    }

#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
    err = CO_storageBlank_init(&storage, CO->CANmodule, OD_ENTRY_H1010_storeParameters,
                               OD_ENTRY_H1011_restoreDefaultParameters, storageEntries, storageEntriesCount,
                               &storageInitError);

    if (err != CO_ERROR_NO && err != CO_ERROR_DATA_CORRUPT) {
        log_printf("Error: Storage %d\n", storageInitError);
        return 0;
    }
#endif

    while (reset != CO_RESET_APP) {
        log_printf("CANopenNode - Reset communication...\n");

        CO->CANmodule->CANnormal = false;
        CO_CANsetConfigurationMode((void*)&CANptr);
        CO_CANmodule_disable(CO->CANmodule);

        err = CO_CANinit(CO, CANptr, pendingBitRate);
        if (err != CO_ERROR_NO) {
            log_printf("Error: CAN initialization failed: %d\n", err);
            return 0;
        }

        CO_LSS_address_t lssAddress = {.identity = {.vendorID = OD_PERSIST_COMM.x1018_identity.vendor_ID,
                                                    .productCode = OD_PERSIST_COMM.x1018_identity.productCode,
                                                    .revisionNumber = OD_PERSIST_COMM.x1018_identity.revisionNumber,
                                                    .serialNumber = OD_PERSIST_COMM.x1018_identity.serialNumber}};
        err = CO_LSSinit(CO, &lssAddress, &pendingNodeId, &pendingBitRate);
        if (err != CO_ERROR_NO) {
            log_printf("Error: LSS slave initialization failed: %d\n", err);
            return 0;
        }

        activeNodeId = pendingNodeId;
        uint32_t errInfo = 0;

        err = CO_CANopenInit(CO, NULL, NULL, OD, OD_STATUS_BITS, NMT_CONTROL,
                             FIRST_HB_TIME, SDO_SRV_TIMEOUT_TIME, 
                             SDO_CLI_TIMEOUT_TIME, SDO_CLI_BLOCK,
                             activeNodeId, &errInfo);
        if (err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) {
            if (err == CO_ERROR_OD_PARAMETERS) {
                log_printf("Error: Object Dictionary entry 0x%X\n", errInfo);
            } else {
                log_printf("Error: CANopen initialization failed: %d\n", err);
            }
            return 0;
        }

        err = CO_CANopenInitPDO(CO, CO->em, OD, activeNodeId, &errInfo);
        if (err != CO_ERROR_NO) {
            if (err == CO_ERROR_OD_PARAMETERS) {
                log_printf("Error: Object Dictionary entry 0x%X\n", errInfo);
            } else {
                log_printf("Error: PDO initialization failed: %d\n", err);
            }
            return 0;
        }

        if (!CO->nodeIdUnconfigured) {
#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
            if (storageInitError != 0) {
                CO_errorReport(CO->em, CO_EM_NON_VOLATILE_MEMORY, CO_EMC_HARDWARE, storageInitError);
            }
#endif
        } else {
            log_printf("CANopenNode - Node-id not initialized\n");
        }

        CO_CANsetNormalMode(CO->CANmodule);
        reset = CO_RESET_NOT;

        log_printf("CANopenNode - Running...\n");
        fflush(stdout);

        /* ----------- aplikacijski kod ----------- */
        static int32_t testTemperature = 20;

        while (reset == CO_RESET_NOT) {
            uint32_t timeDifference_us = 500;

            reset = CO_process(CO, false, timeDifference_us, NULL);
            LED_red = CO_LED_RED(CO->LEDs, CO_LED_CANopen);
            LED_green = CO_LED_GREEN(CO->LEDs, CO_LED_CANopen);

            /* simulacija senzora: povecavamo temperaturu od 20 do 40 */
            testTemperature++;
            if(testTemperature > 40) testTemperature = 20;

            OD_RAM.x2000_temperature = testTemperature;

            log_printf("Temperatura = %d C\n", OD_RAM.x2000_temperature);
            log_printf("Addr of OD_RAM.x2000_temperature = %p, value = %d\n",
           (void*)&OD_RAM.x2000_temperature,
           OD_RAM.x2000_temperature);

        }
    }

    CO_CANsetConfigurationMode((void*)&CANptr);
    CO_delete(CO);

    log_printf("CANopenNode finished\n");
    return 0;
}

/* timer thread executes in constant intervals ********************************/
void
tmrTask_thread(void) {
    for (;;) {
        CO_LOCK_OD(CO->CANmodule);
        if (!CO->nodeIdUnconfigured && CO->CANmodule->CANnormal) {
            bool_t syncWas = false;
            uint32_t timeDifference_us = 1000;

#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
            syncWas = CO_process_SYNC(CO, timeDifference_us, NULL);
#endif
#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
            CO_process_RPDO(CO, syncWas, timeDifference_us, NULL);
#endif
#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
            CO_process_TPDO(CO, syncWas, timeDifference_us, NULL);
#endif
        }
        CO_UNLOCK_OD(CO->CANmodule);
    }
}

/* CAN interrupt function executes on received CAN message ********************/
void /* interrupt */
CO_CAN1InterruptHandler(void) {
    /* clear interrupt flag */
}

