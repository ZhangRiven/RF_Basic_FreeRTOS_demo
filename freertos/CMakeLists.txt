#############################################################[Chinese Manual]###################################################################
# *****工程路径不要包含中文字符和空格,否则可能会报错！*****
# 第一步： 确认机器上已经安装了MRS的工具链(假设当前平台为Linux,工具链路径为"/path/to/toolchain/RISC-V Embedded GCC12",工具链相关前缀为"riscv-wch-elf-")
# 第二步: 设置工具链路径,例如修改替换下方的set(TOOLCHAIN_FOLDER "/path/to/toolchain/RISC-V Embedded GCC12")
# 第三步: 设置工具链中相关可执行文件的前缀,例如修改替换下方的set(TOOLCHAIN_PREFIX "riscv-wch-elf-")
# 第四步: 执行 cd your/path/of/project
# 第五步: 执行 cmake -B build -G "Unix Makefiles"
# 第六步: 执行 cmake --build build
# 注意: 在Windows平台运行,需要a.安装GNU MAKE并将其路径(bin目录)添加到系统环境变量,之后执行cmake -B build -G "Unix Makefiles"(推荐)
#       或者b.安装MinGW,将其路径(bin目录)添加到系统环境变量,之后执行cmake -B build -G "MinGW Makefiles"
# 如有相关疑问,可通过support@mounriver.com联系我们
#################################################################################################################################################


#############################################################[English Manual]###################################################################
# Step 1: Confirm that the MRS toolchain has been installed on the machine. Assuming the current platform is Linux, 
#         the toolchain path is "/path/to/toolchain/RISC-V Embedded GCC12", and the toolchain-related prefix is "riscv-wch-elf-"
# Step 2: Set the toolchain path, for example, modify and replace the following "set (TOOLCHAIN_FOLDER "/path/to/toolchain/RISC-V Embedded GCC12")"
# Step 3: Set the prefix of the executable files in the toolchain,for example, modify and replace the following "set(TOOLCHAIN_PREFIX "riscv-wch-elf-")"
# Step 4: Call "cd your/path/of/project"
# Step 5: Call "cmake -B build -G "Unix Makefiles""
# Step 6: Call "cmake --build build"
# Note:   To run on Windows, you need: a. Install GNU MAKE and add its path (the bin directory) to the system environment PATH. 
#         Then call the execution of "cmake -B build -G "Unix Makefiles""(Recommanded).
#         or b. Install MinGW and add its path (the bin directory) to the system environment PATH. 
#         Then call the execution of "cmake -B build -G "MinGW Makefiles"".
# If you have any questions, please contact us at support@mounriver.com
#################################################################################################################################################


# Set CMake minimum require
cmake_minimum_required(VERSION 3.16)

# Set CMake sysytem name
set(CMAKE_SYSTEM_NAME Generic)


# Skipping compiler detection and use custom toolchain
SET(CMAKE_C_COMPILER_WORKS TRUE)
SET(CMAKE_CXX_COMPILER_WORKS TRUE)
SET(CMAKE_ASM_COMPILER_WORKS TRUE)

# Use MRS toolchian to compile
set(TOOLCHAIN_FOLDER "/opt/gcc-riscv-none-elf")
set(TOOLCHAIN_PREFIX "riscv-none-elf-")
set(CMAKE_C_COMPILER "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}g++")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_OBJCOPY "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}objcopy")
set(CMAKE_OBJDUMP "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}objdump")
set(CMAKE_SIZE "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}size")
set(CMAKE_AR "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}ar")

set(PROJECT_NAME "RF_Basic_FreeRTOS")
set(TARGET_ARTIFACT "RF_Basic_FreeRTOS.elf")
set(RF_TEST_MODE "1" CACHE STRING "RF test mode: 0=RX, 1=TX")

set(CMAKE_C_OUTPUT_EXTENSION .o)
set(CMAKE_CXX_OUTPUT_EXTENSION .o)

project("${PROJECT_NAME}" LANGUAGES C ASM CXX)

# The vendor archive combines RFRole with the BLE/TMOS stack.  Extract its
# members into the build tree so the RF-only link can explicitly exclude the
# TMOS scheduler and BLE task dispatcher.
set(RF_VENDOR_OBJECT_DIR "${CMAKE_BINARY_DIR}/rf_vendor_objects")
file(MAKE_DIRECTORY "${RF_VENDOR_OBJECT_DIR}")
execute_process(
    COMMAND ${CMAKE_AR} x "${CMAKE_CURRENT_SOURCE_DIR}/../rf_basic/LIB/libCH58xBLE.a"
    WORKING_DIRECTORY "${RF_VENDOR_OBJECT_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)
file(GLOB RF_VENDOR_OBJECTS "${RF_VENDOR_OBJECT_DIR}/*.o")
list(FILTER RF_VENDOR_OBJECTS EXCLUDE REGEX "/(tmos|tmos_hw|tmos_snv|ble_task_scheduler|ble_task_process)\\.o$")

# Use relative path
add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=..)


set(SRC_FILES 
"${CMAKE_CURRENT_SOURCE_DIR}/src/rf_main.c"
"${CMAKE_CURRENT_SOURCE_DIR}/src/tmos_shim.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Startup/Startup_CH585_FreeRTOS.S"
"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/portable/MemMang/heap_4.c"
"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/port.c"
"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/portASM.S"
"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/croutine.c"
"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/event_groups.c"
"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/list.c"
"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/queue.c"
"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/stream_buffer.c"
"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/tasks.c"
"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/timers.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_adc.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_clk.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_flash.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_gpio.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_i2c.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_lcd.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_pwm.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_pwr.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_spi0.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_spi1.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_sys.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_timer0.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_timer1.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_timer2.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_timer3.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_uart0.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_uart1.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_uart2.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_uart3.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_usbdev.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_usbhostBase.c"
"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/CH58x_usbhostClass.c"
${RF_VENDOR_OBJECTS}
)
set(CMAKE_C_FLAGS " -march=rv32imc_zba_zbb_zbc_zbs_xw -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common --param=highcode-gen-section-name=1 -g -I\"${CMAKE_CURRENT_SOURCE_DIR}/Startup\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/portable\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/include\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/RVMSIS/\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/inc\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/../rf_basic/LIB\" -DDEBUG=0 -DRF_TEST_MODE=${RF_TEST_MODE} -std=gnu99")
set(CMAKE_CXX_FLAGS " -march=rv32imc_zba_zbb_zbc_zbs_xw -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common --param=highcode-gen-section-name=1 -g -std=gnu++11 -fabi-version=0")
set(CMAKE_ASM_FLAGS " -march=rv32imc_zba_zbb_zbc_zbs_xw -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common --param=highcode-gen-section-name=1 -g -I\"${CMAKE_CURRENT_SOURCE_DIR}/RVMSIS/\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/Startup\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V\" -x assembler-with-cpp -DENABLE_INTERRUPT_NEST=1")

add_executable("${TARGET_ARTIFACT}" ${SRC_FILES})

set_target_properties("${TARGET_ARTIFACT}" PROPERTIES LINK_FLAGS " -march=rv32imc_zba_zbb_zbc_zbs_xw -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common --param=highcode-gen-section-name=1 -g -T \"${CMAKE_CURRENT_SOURCE_DIR}/Ld/Link.ld\"  -L\"${CMAKE_CURRENT_SOURCE_DIR}/\" -L\"${CMAKE_CURRENT_SOURCE_DIR}/StdPeriphDriver/\" -L\"${CMAKE_CURRENT_SOURCE_DIR}/../rf_basic/LIB\" -nostartfiles -Xlinker --gc-sections -Xlinker --print-memory-usage -Wl,-Map,\"RF_Basic_FreeRTOS.map\" --specs=nano.specs --specs=nosys.specs -o \"RF_Basic_FreeRTOS.elf\" ")

target_link_libraries(
"${TARGET_ARTIFACT}" 
ISP585
)

add_custom_command(TARGET "${TARGET_ARTIFACT}" 
POST_BUILD 
COMMAND ${CMAKE_OBJCOPY} ARGS -O ihex  "RF_Basic_FreeRTOS.elf" "RF_Basic_FreeRTOS.hex"
COMMAND ${CMAKE_OBJDUMP} ARGS  --all-headers --demangle --disassemble -M xw --line-numbers -D "RF_Basic_FreeRTOS.elf" > "RF_Basic_FreeRTOS.lst"
COMMAND ${CMAKE_SIZE} ARGS  --format=berkeley "RF_Basic_FreeRTOS.elf"
)

