#include "daemon.h"

#define DISABLE_PROCESS_CALLS

#define BACKLIGHT_CONTROL_FILE "/sys/class/backlight/fde30000.dsi.0/brightness"
#define BRIGHTNESS_RANGE_MIN 0
#define BRIGHTNESS_RANGE_MAX 100

#define BRIGHTNESS_LEVEL_MIN 0
#define BRIGHTNESS_LEVEL_MAX 255

#define MIN_ALLOWED_BRIGHTNESS_LEVEL 13 // min allowed level is BRIGHTNESS_LEVEL_MIN + (5% of BRIGHTNESS_LEVEL_MAX)
#define MAX_ALLOWED_BRIGHTNESS_LEVEL 243 // max allowed level is BRIGHTNESS_LEVEL_MAX - (5% of BRIGHTNESS_LEVEL_MAX)

#define m_scaleFactor 2.3 // (MAX_ALLOWED_BRIGHTNESS_LEVEL - MIN_ALLOWED_BRIGHTNESS_LEVEL) / (BRIGHTNESS_RANGE_MAX - BRIGHTNESS_RANGE_MIN)

Daemon::Daemon()
{
    new EmbeddedInterfaceAdaptor(this);
    QDBusConnection connection = QDBusConnection::systemBus();
    connection.registerObject("/com/monero/nodo", this);
    connection.registerService("com.monero.nodo");

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(updateParams()));
    m_timer->start(3000);

}

void Daemon::startRecovery(int recoverFS, int rsyncBlockchain)
{
    qDebug() << QString("received recovery request").append("recoverFS: ").append(QString::number(recoverFS).append(" rsyncBlockchain: ").append(QString::number(rsyncBlockchain)));

    QString program = "/usr/bin/recovery.sh";
    QStringList arguments;

    if(1 == recoverFS)
    {
        arguments << "-repair";
    }

    if(1 == rsyncBlockchain)
    {
        arguments << "-purge";
    }

    QProcess process;
    process.start(program, arguments);
    process.waitForFinished(-1);
    emit startRecoveryNotification("startRecovery initiated");
}

const static QString operations[] = {"start", "stop", "restart", "enable", "disable"};
const static QString services[] = {"block-explorer", "monerod", "monero-lws", "xmrig", "sshd"};

void Daemon::serviceManager(QString operation, QString service)
{
    qDebug() << QString("serviceManager ").append(operation).append(" initiated");

    QString program = "/usr/bin/systemctl";

    bool valid = false;
    for (uint i = 0; i < 5; i++)
    {
      if (operation == operations[i])
      {
        valid = true;
        break;
      }
    }
    if (!valid)
    {
        qDebug() << "illegal operation: " << operation;
        emit serviceManagerNotification("illegal operation (either one of: start, stop, restart, enable, disable).");
        return;
    }


    QStringList service_list = service.split(' ');
    for (QString s : service_list)
    {
      valid = false;
      for (int i = 0; i < 5; i++)
      {
        if (s == services[i] || s == services[i] + ".service")
        {
          valid = true;
          break;
        }
      }
      if (!valid)
      {
          qDebug() << "illegal service: " << s;
          emit serviceManagerNotification("illegal service.");
          return;
      }
    }

    QStringList arguments;

    arguments << operation << service_list;

    QProcess process;
    process.start(program, arguments);
    if(!process.waitForStarted())
    {
        qDebug() << QString("failed to ").append(operation).append(" service").append(". Exiting...");
        emit serviceManagerNotification(QString("failed to ").append(operation).append(" service ").append(". Exiting..."));

        return;
    }

    if(!process.waitForFinished())
    {
        qDebug() << QString("failed to ").append(operation).append(" service ").append(". Exiting...");
        emit serviceManagerNotification(QString("failed to ").append(operation).append("service ").append(". Exiting..."));

        return;
    }

    qDebug() << "success";
    emit serviceManagerNotification("success");
}

void Daemon::restart(void)
{
    qDebug() << "received restart request";

    QString program = "/usr/bin/systemctl";

    QStringList arguments;
    arguments << "reboot";

    QProcess process;

    process.start(program, arguments);
    process.waitForFinished(-1);

    emit restartNotification("received restart request");
}

void Daemon::shutdown(void)
{
    qDebug() << "received shutdown request";

    QString program = "/usr/bin/systemctl";

    QStringList arguments;
    arguments << "poweroff";

    QProcess process;

    process.start(program, arguments);
    process.waitForFinished(-1);
    emit shutdownNotification("received shutdown request");
}

void Daemon::setBacklightLevel(int backlightLevel)
{
    if(backlightLevel < BRIGHTNESS_RANGE_MIN)
    {
        backlightLevel = BRIGHTNESS_RANGE_MIN;
    }

    if(backlightLevel > BRIGHTNESS_RANGE_MAX)
    {
        backlightLevel = BRIGHTNESS_RANGE_MAX;
    }

    int blLevel = (int)(MIN_ALLOWED_BRIGHTNESS_LEVEL + backlightLevel*m_scaleFactor);

    QString filename = BACKLIGHT_CONTROL_FILE;
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        stream << QString::number(blLevel);
    }
    else {
        qDebug() << "file open error";
    }
    file.close();
}

int Daemon::getBacklightLevel(void)
{
    int retVal = BRIGHTNESS_RANGE_MAX/2;

    QFile file(BACKLIGHT_CONTROL_FILE);
    if (!file.open(QIODevice::ReadWrite))
    {
        qDebug() << "file read error";
        return BRIGHTNESS_RANGE_MAX;
    }

    while (!file.atEnd()) {
        bool ok;

        QByteArray line = file.readLine();
        retVal = line.toInt(&ok, 10);

        return (int)(retVal/m_scaleFactor);
        file.close();
    }

    if(file.isOpen())
        file.close();

    return retVal;
}

void Daemon::getServiceStatus(void)
{
    qDebug() << "received service status request";

    QString program = "/usr/bin/systemctl";
    QString serviceList[] = {"monerod", "miner", "tor", "i2p", "monero-lws", "block-explorer", "sshd"};

    QStringList arguments;
    arguments << "is-active" << serviceList[0] << serviceList[1] << serviceList[2] << serviceList[3] << serviceList[4] << serviceList[5] << serviceList[6];

    QProcess process;

    process.start(program, arguments);
    process.waitForFinished(-1);
    QString retVal = process.readAll();
    QStringList status = retVal.split("\n", Qt::SkipEmptyParts);
    QString statusMessage;

    for(int i = 0; i < status.size(); i++)
    {
        statusMessage.append(serviceList[i]).append(":").append(status.at(i)).append("\n");
    }

    qDebug() << statusMessage;
    emit serviceStatusReadyNotification(statusMessage);
}

void Daemon::updateParams(void)
{
    readCPUUsage();
    readAverageCPUFreq();
    readRAMUsage();
    readCPUTemperature();
    readBlockchainStorageUsage();
    readSystemStorageUsage();
}

void Daemon::readCPUUsage(void)
{
    QFile file("/proc/stat");
    file.open(QFile::ReadOnly);
    const QList<QByteArray> times = file.readLine().simplified().split(' ').mid(1);
    const int idleTime = times.at(3).toInt();
    int totalTime = 0;
    foreach (const QByteArray &time, times) {
        totalTime += time.toInt();
    }

    m_CPUUsage = (1 - (1.0*idleTime-m_prevIdleTime) / (totalTime-m_prevTotalTime)) * 100.0;

    m_prevIdleTime = idleTime;
    m_prevTotalTime = totalTime;
}

void Daemon::readAverageCPUFreq(void)
{
    QProcess process;
    QString program = "/usr/bin/cat";


    for(int i = 0; i < 8; i++)
    {
        QStringList arguments;
        QString path("/sys/devices/system/cpu/cpu");
        path.append(QString::number(i)).append("/cpufreq/cpuinfo_cur_freq");
        // path.append(QString::number(i)).append("/cpufreq/scaling_cur_freq");
        arguments << path;

        process.start(program, arguments);
        process.waitForFinished(10000);
        process.waitForReadyRead(10000);

        QString retVal = process.readAll();

        QStringList status = retVal.split("\n", Qt::SkipEmptyParts);
        bool ok;
        m_AverageCPUFreq += status.at(0).toDouble(&ok)/1000;
    }

    m_AverageCPUFreq = m_AverageCPUFreq/8;
}

void Daemon::readRAMUsage(void)
{
    QProcess process;
    QString program = "/usr/bin/free";
    QStringList arguments;
    arguments << "-h" << "--si";


    process.start(program, arguments);
    process.waitForFinished(-1);
    QString retVal = process.readAll();

    QStringList status = retVal.split("\n", Qt::SkipEmptyParts);

    for(int i = 0; i < status.size(); i++)
    {
        if(-1 != status.at(i).indexOf("Mem:"))
        {
            bool ok;
            QStringList status2 = status.at(i).split(" ", Qt::SkipEmptyParts);
            m_TotalRAM = status2.at(1).chopped(1).toFloat(&ok);
            m_RAMUsage = status2.at(2).chopped(1).toFloat(&ok);
        }
    }
}

void Daemon::readCPUTemperature(void)
{
    QProcess process;
    QString program = "/usr/bin/cat";
    QStringList arguments;
    arguments << "/sys/devices/virtual/thermal/thermal_zone0/temp";

    process.start(program, arguments);
    process.waitForFinished(-1);
    QString retVal = process.readAll();
    bool ok;
    m_CPUTemperature = retVal.toFloat(&ok)/(1000.0);
}

void Daemon::readBlockchainStorageUsage(void)
{
    QProcess process;
    QString program = "/usr/bin/df";
    QStringList arguments;
    arguments << "-h";

    process.start(program, arguments);
    process.waitForFinished(-1);
    QString retVal = process.readAll();

    QStringList status = retVal.split("\n", Qt::SkipEmptyParts);

    for(int i = 0; i < status.size(); i++)
    {
        if(-1 != status.at(i).indexOf("/dev/nvme0n1p1"))
        {
            bool ok;
            QStringList status2 = status.at(i).split(" ", Qt::SkipEmptyParts);
            m_blockChainStorageTotal = status2.at(1).chopped(1).toFloat(&ok);
            m_blockChainStorageUsed = status2.at(2).chopped(1).toFloat(&ok);
        }
    }
}

void Daemon::readSystemStorageUsage(void)
{
    QProcess process;
    QString program = "/usr/bin/df";
    QStringList arguments;
    arguments << "-h";

    process.start(program, arguments);
    process.waitForFinished(-1);
    QString retVal = process.readAll();

    QStringList status = retVal.split("\n", Qt::SkipEmptyParts);

    for(int i = 0; i < status.size(); i++)
    {
        if(-1 != status.at(i).indexOf("overlay"))
        // if(-1 != status.at(i).indexOf("/dev/nvme0n1p2"))
        {
            bool ok;
            QStringList status2 = status.at(i).split(" ", Qt::SkipEmptyParts);
            m_systemStorageTotal = status2.at(1).chopped(1).toFloat(&ok);
            m_systemStorageUsed = status2.at(2).chopped(1).toFloat(&ok);
        }
    }
}


double Daemon::getCPUUsage(void)
{
    return m_CPUUsage;
}

double Daemon::getAverageCPUFreq(void)
{
    return m_AverageCPUFreq;
}

double Daemon::getRAMUsage(void)
{
    return m_RAMUsage;
}

double Daemon::getTotalRAM(void)
{
    return m_TotalRAM;
}

double Daemon::getCPUTemperature(void)
{
    return m_CPUTemperature;
}

double Daemon::getBlockchainStorageUsage(void)
{
    return m_blockChainStorageUsed;
}

double Daemon::getTotalBlockchainStorage(void)
{
    return m_blockChainStorageTotal;
}

double Daemon::getSystemStorageUsage(void)
{
    return m_systemStorageUsed;
}

double Daemon::getTotalSystemStorage(void)
{
    return m_systemStorageTotal;
}

void Daemon::setPassword(QString pw)
{
    QProcess sh;
    QString tmp = QString("echo \"nodo:").append(pw).append("\"  | chpasswd");
    sh.start( "sh", { "-c", tmp});
    sh.waitForFinished( -1 );

    sh.start( "sh", { "-c", "echo \"root:$(openssl rand -base64 48)\" | chpasswd" } );
    sh.waitForFinished( -1 );
}
