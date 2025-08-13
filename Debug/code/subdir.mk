################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../code/image_storage.c" \
"../code/w25n04.c" 

COMPILED_SRCS += \
"code/image_storage.src" \
"code/w25n04.src" 

C_DEPS += \
"./code/image_storage.d" \
"./code/w25n04.d" 

OBJS += \
"code/image_storage.o" \
"code/w25n04.o" 


# Each subdirectory must supply rules for building sources it contributes
"code/image_storage.src":"../code/image_storage.c" "code/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2004 -D__CPU__=tc26xb "-fF:/Smart_car/code/flash/flash_V1.0.0/Debug/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc26xb -Y0 -N0 -Z0 -o "$@" "$<"
"code/image_storage.o":"code/image_storage.src" "code/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"
"code/w25n04.src":"../code/w25n04.c" "code/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2004 -D__CPU__=tc26xb "-fF:/Smart_car/code/flash/flash_V1.0.0/Debug/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc26xb -Y0 -N0 -Z0 -o "$@" "$<"
"code/w25n04.o":"code/w25n04.src" "code/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-code

clean-code:
	-$(RM) ./code/image_storage.d ./code/image_storage.o ./code/image_storage.src ./code/w25n04.d ./code/w25n04.o ./code/w25n04.src

.PHONY: clean-code

