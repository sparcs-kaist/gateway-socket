################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../arp.cpp \
../database.cpp \
../dhcp.cpp \
../ethernet.cpp \
../gateway.cpp \
../ip.cpp \
../main.cpp \
../packet.cpp \
../socket.cpp \
../udp.cpp 

OBJS += \
./arp.o \
./database.o \
./dhcp.o \
./ethernet.o \
./gateway.o \
./ip.o \
./main.o \
./packet.o \
./socket.o \
./udp.o 

CPP_DEPS += \
./arp.d \
./database.d \
./dhcp.d \
./ethernet.d \
./gateway.d \
./ip.d \
./main.d \
./packet.d \
./socket.d \
./udp.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/usr/include -I/usr/include/c++/4.7.3 -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


