Import("env")

env.Replace(
    LINKFLAGS=[
            "-mthumb",
            "-mthumb-interwork",
            "--specs=nano.specs",
            "--specs=nosys.specs",
            "-flto",
            "-mcpu=cortex-m3",
            "-Wl,--gc-sections,--relax,-eReset_Handler,-T${PROJECT_DIR}/LPC176X.ld",
    ]
)

# Custom HEX from ELF
env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.elf",
    env.VerboseAction(" ".join([
        "$OBJCOPY", "-O", "ihex", "-R", ".eeprom",
        "$BUILD_DIR/${PROGNAME}.elf", "$BUILD_DIR/${PROGNAME}.hex"
    ]), "Building $BUILD_DIR/${PROGNAME}.hex")
)