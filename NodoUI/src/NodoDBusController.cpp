#include "NodoDBusController.h"
#include <QDate>
NodoDBusController::NodoDBusController(QObject *parent) : QObject{parent}
{
    m_connectionStatus = false;
    nodo = new com::moneronodo::embeddedInterface("com.monero.nodo", "/com/monero/nodo", QDBusConnection::systemBus(), this);
    connect(nodo, SIGNAL(serviceManagerNotification(QString)), this, SIGNAL(serviceManagerNotificationReceived(QString)));
    connect(nodo, SIGNAL(serviceStatusReadyNotification(QString)), this, SLOT(updateServiceStatus(QString)));
    connect(nodo, SIGNAL(networkConfigurationChanged()), this, SLOT(updateNetworkConfiguration()));

    startTimer(1000);
}

void NodoDBusController::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);

    bool previousState = m_connectionStatus;
    if (nodo->isValid())
    {
        m_connectionStatus = true;
    }
    else
    {
        m_connectionStatus = false;
    }
    if(previousState == m_connectionStatus)
    {
        emit connectionStatusChanged();
    }
}

bool NodoDBusController::isConnected()
{
    return m_connectionStatus;
}

void NodoDBusController::startRecovery(int recoverFS, int rsyncBlockchain)
{
    nodo->startRecovery(recoverFS, rsyncBlockchain);
}

void NodoDBusController::serviceManager(QString operation, QString service)
{
    nodo->serviceManager(operation, service);
}

void NodoDBusController::restart()
{
    nodo->restart();
}

void NodoDBusController::shutdown()
{
    nodo->shutdown();
}

void NodoDBusController::updateTextEdit(QString message)
{
    Q_UNUSED(message);
}

void NodoDBusController::setBacklightLevel(int backlightLevel)
{
    nodo->setBacklightLevel(backlightLevel);
}

int NodoDBusController::getBacklightLevel(void)
{
    return nodo->getBacklightLevel();
}

void NodoDBusController::updateServiceStatus(QString message)
{
    emit serviceStatusReceived(message);
}
void NodoDBusController::getServiceStatus(void)
{
    nodo->getServiceStatus();
}

double NodoDBusController::getCPUUsage(void)
{
    return nodo->getCPUUsage();
}

double NodoDBusController::getAverageCPUFreq(void)
{
    return nodo->getAverageCPUFreq();
}

double NodoDBusController::getRAMUsage(void)
{
    return nodo->getRAMUsage();
}

double NodoDBusController::getTotalRAM(void)
{
    return nodo->getTotalRAM();
}

double NodoDBusController::getCPUTemperature(void)
{
    return nodo->getCPUTemperature();
}

double NodoDBusController::getBlockchainStorageUsage(void)
{
    return nodo->getBlockchainStorageUsage();
}

double NodoDBusController::getTotalBlockchainStorage(void)
{
    return nodo->getTotalBlockchainStorage();
}

double NodoDBusController::getSystemStorageUsage(void)
{
    return nodo->getSystemStorageUsage();
}

double NodoDBusController::getTotalSystemStorage(void)
{
    return nodo->getTotalSystemStorage();
}

void NodoDBusController::setPassword(QString pw)
{
    nodo->setPassword(pw);
}

QString NodoDBusController::getConnectedDeviceConfig(void)
{
    m_networkConfig.clear();
    m_networkConfig = nodo->getConnectedDeviceConfig();

    return m_networkConfig;
}

void NodoDBusController::updateNetworkConfiguration(void)
{
    getConnectedDeviceConfig();
    emit newNetworkConfigurationReceived();
}
