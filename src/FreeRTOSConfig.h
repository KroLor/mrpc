/*
 * FreeRTOS Configured for Windows Simulator (MinGW/MSVC)
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/**
 * @file FreeRTOSConfig.h
 * @brief Конфигурационный файл FreeRTOS для симулятора под Windows.
 * 
 * Определяет параметры ядра FreeRTOS: приоритеты, размеры стека, таймеры,
 * семафоры и другие настройки для работы в среде Windows (MinGW/MSVC).
 */

/* Основные настройки планировщика */
#define configUSE_PREEMPTION                    1   ///< Вытесняющая многозадачность
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1   ///< Оптимизированный выбор задачи
#define configUSE_IDLE_HOOK                     0   ///< Хук задачи idle отключен
#define configUSE_TICK_HOOK                     0   ///< Хук системного тика отключен
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0   ///< Хук запуска демон-задачи отключен
#define configTICK_RATE_HZ                      ( 1000 ) ///< Частота системного тика (Гц)
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 70 ) ///< Минимальный размер стека
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 100 * 1024 ) ) ///< Общий размер кучи (байты)
#define configMAX_TASK_NAME_LEN                 ( 12 ) ///< Максимальная длина имени задачи
#define configUSE_TRACE_FACILITY                0   ///< Трассировка отключена
#define configIDLE_SHOULD_YIELD                 1   ///< Idle задача уступает процессор
#define configUSE_MUTEXES                       1   ///< Мьютексы включены
#define configCHECK_FOR_STACK_OVERFLOW          0   ///< Проверка переполнения стека отключена
#define configUSE_RECURSIVE_MUTEXES             1   ///< Рекурсивные мьютексы включены
#define configQUEUE_REGISTRY_SIZE               20  ///< Размер реестра очередей
#define configUSE_APPLICATION_TASK_TAG          0   ///< Теги задач отключены
#define configUSE_COUNTING_SEMAPHORES           1   ///< Счетные семафоры включены
#define configUSE_ALTERNATIVE_API               0   ///< Альтернативный API отключен
#define configUSE_QUEUE_SETS                    1   ///< Наборы очередей включены
#define configUSE_TASK_NOTIFICATIONS            1   ///< Уведомления задач включены
#define configSUPPORT_STATIC_ALLOCATION         0   ///< Статическое выделение памяти отключено

/* Определение разрядности типа Tick */
#ifdef __x86_64__
    #define configTICK_TYPE_WIDTH_IN_BITS       TICK_TYPE_WIDTH_64_BITS
#else
    #define configTICK_TYPE_WIDTH_IN_BITS       TICK_TYPE_WIDTH_32_BITS
#endif

/* Настройки программных таймеров */
#define configUSE_TIMERS                        1   ///< Таймеры включены
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 ) ///< Приоритет задачи таймера
#define configTIMER_QUEUE_LENGTH                20  ///< Длина очереди таймера
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 ) ///< Размер стека задачи таймера

#define configMAX_PRIORITIES                    ( 7 ) ///< Максимальное количество приоритетов

/* Отключаем сбор статистики времени выполнения для упрощения сборки */
#define configGENERATE_RUN_TIME_STATS           0   ///< Статистика времени выполнения отключена
#define configUSE_CO_ROUTINES                   0   ///< Корутины отключены
#define configUSE_STATS_FORMATTING_FUNCTIONS    0   ///< Функции форматирования статистики отключены
#define configSTACK_DEPTH_TYPE                  uint32_t ///< Тип глубины стека

/* Включение необходимых функций API */
#define INCLUDE_vTaskPrioritySet                1   ///< Функция vTaskPrioritySet включена
#define INCLUDE_uxTaskPriorityGet               1   ///< Функция uxTaskPriorityGet включена
#define INCLUDE_vTaskDelete                     1   ///< Функция vTaskDelete включена
#define INCLUDE_vTaskCleanUpResources           0   ///< Функция vTaskCleanUpResources отключена
#define INCLUDE_vTaskSuspend                    1   ///< Функция vTaskSuspend включена
#define INCLUDE_vTaskDelayUntil                 1   ///< Функция vTaskDelayUntil включена
#define INCLUDE_vTaskDelay                      1   ///< Функция vTaskDelay включена
#define INCLUDE_uxTaskGetStackHighWaterMark     1   ///< Функция uxTaskGetStackHighWaterMark включена
#define INCLUDE_uxTaskGetStackHighWaterMark2    1   ///< Функция uxTaskGetStackHighWaterMark2 включена
#define INCLUDE_xTaskGetSchedulerState          1   ///< Функция xTaskGetSchedulerState включена
#define INCLUDE_xTimerGetTimerDaemonTaskHandle  1   ///< Функция xTimerGetTimerDaemonTaskHandle включена
#define INCLUDE_xTaskGetIdleTaskHandle          1   ///< Функция xTaskGetIdleTaskHandle включена
#define INCLUDE_xTaskGetHandle                  1   ///< Функция xTaskGetHandle включена
#define INCLUDE_eTaskGetState                   1   ///< Функция eTaskGetState включена
#define INCLUDE_xSemaphoreGetMutexHolder        1   ///< Функция xSemaphoreGetMutexHolder включена
#define INCLUDE_xTimerPendFunctionCall          1   ///< Функция xTimerPendFunctionCall включена
#define INCLUDE_xTaskAbortDelay                 1   ///< Функция xTaskAbortDelay включена

#define configINCLUDE_MESSAGE_BUFFER_AMP_DEMO   0   ///< Демо сообщение буфера отключено
#define configUSE_MALLOC_FAILED_HOOK            0   ///< Хук ошибки malloc отключен

/* Стандартная безопасная проверка ошибок (Assert) */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

#endif /* FREERTOS_CONFIG_H */