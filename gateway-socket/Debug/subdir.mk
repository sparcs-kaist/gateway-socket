################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../arp.cpp \
../ethernet.cpp \
../gateway.cpp \
../main.cpp \
../packet.cpp \
../socket.cpp 

OBJS += \
./arp.o \
./ethernet.o \
./gateway.o \
./main.o \
./packet.o \
./socket.o 

CPP_DEPS += \
./arp.d \
./ethernet.d \
./gateway.d \
./main.d \
./packet.d \
./socket.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/usr/include -I/usr/include/c++/4.7.3 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


