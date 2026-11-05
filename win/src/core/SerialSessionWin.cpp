#include "protocol/serialSession/serialSession.h"

SerialSession::SerialSession() {
    setError(SERIAL_OPEN_FAILED, "Windows 版暂不支持串口");
}

SerialSession::~SerialSession() {
    running.store(false);
    if (receiveThread.joinable()) receiveThread.join();
    if (serialFd >= 0) {
        close(serialFd);
        serialFd = -1;
    }
}

bool SerialSession::openConnect(const std::string& napiInput) {
    (void)napiInput;
    setError(SERIAL_OPEN_FAILED, "Windows 版暂不支持串口");
    return false;
}

bool SerialSession::disconnect() {
    running.store(false);
    if (receiveThread.joinable()) receiveThread.join();
    if (serialFd >= 0) {
        close(serialFd);
        serialFd = -1;
    }
    return true;
}

bool SerialSession::sendData(const std::string& command) {
    (void)command;
    setError(SERIAL_SEND_FAILED, "Windows 版暂不支持串口");
    return false;
}

bool SerialSession::setLogging(int fd, int logType, int existOperation, bool includeHeader,
                               bool omitKnownPassword, bool omitSessionData) {
    (void)fd;
    (void)logType;
    (void)existOperation;
    (void)includeHeader;
    (void)omitKnownPassword;
    (void)omitSessionData;
    return false;
}

bool SerialSession::resizeTerminal(int width, int height) {
    (void)width;
    (void)height;
    return false;
}

void SerialSession::receiveLoop() {
}

bool SerialSession::configureSerialPort(int baudRateIn, int dataBitsIn, int stopBitsIn,
                                        SerialParity parityIn, SerialFlowControl flowControlIn) {
    (void)baudRateIn;
    (void)dataBitsIn;
    (void)stopBitsIn;
    (void)parityIn;
    (void)flowControlIn;
    return false;
}

void SerialSession::setError(SerialError code, const std::string& message) {
    lastError.code = code;
    lastError.message = message;
}
