#include "widget.h"
#include "ui_widget.h"

#include <QtSerialPort/QSerialPortInfo>
#include <QIntValidator>
#include <QLineEdit>
#include <QFileDialog>
#include <QDataStream>
#include <QFileInfo>
#include <stdio.h>

//#include <QDebug>

QT_USE_NAMESPACE

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    serial = new QSerialPort(this);
    OpenStatus = false;
    intValidator = new QIntValidator(0, 4000000, this);
    fileLocation = "../";
    binLoadCnt = 1;
    usart_temp = 0;

    ui->loadButton->setEnabled (false);

    ui->loadTextEdit->insertPlainText (tr("Instructions:\n"));
    ui->loadTextEdit->insertPlainText (tr("1.Connect usb to computer.\n"));
    ui->loadTextEdit->insertPlainText (tr("2.Select the upgrade file.\n"));
    ui->loadTextEdit->insertPlainText (tr("3.Choose the right COM.\n"));
    ui->loadTextEdit->insertPlainText (tr("4.Click OPEN,you can see \"Detecting...\"\n"));
    ui->loadTextEdit->insertPlainText (tr("5.Waiting for \"Connected !\"\n"));
    ui->loadTextEdit->insertPlainText (tr("6.Click Upload,Waiting for \"Upgrade Complete !\"\n"));

    connect (serial, SIGNAL(readyRead()), this, SLOT(readData()));

    connect (this, SIGNAL(transmitData(int)), this, SLOT(transmitDataFun(int)));
    connect (this, SIGNAL(binFileOpenFailed()), this, SLOT(on_fileButton_clicked()));

    fillPortsCom();
}

Widget::~Widget()
{
    delete ui;
}

void Widget::fillPortsCom ()
{
    ui->uNameBox->clear ();
    static const QString blankString = QObject::tr("N/A");
    QString description;
    QString manufacturer;
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        QStringList list;
        description = info.description();
        manufacturer = info.manufacturer();
        list << info.portName()
             << (!description.isEmpty () ? description : blankString)
             << (!manufacturer.isEmpty () ? manufacturer : blankString)
             << info.systemLocation()
             << (info.vendorIdentifier() ? QString::number (info.vendorIdentifier(), 16) : blankString)
             << (info.productIdentifier() ? QString::number (info.productIdentifier(), 16) : blankString);
        ui->uNameBox->addItem (list.first (), list);
    }
}

void Widget::readData ()
{
    usart_temp += serial->readAll (); //??????????????????
    if(usart_temp == "\r\r\n")//???????????????????????????"\r\r\n"?????????????????????????????????????????????????????????
    {
        //ui->loadTextEdit->insertPlainText (tr("Device detected !\n"));
        serial->write (":upgrade;");
        usart_temp.clear ();
    }
    if(usart_temp == "Ready\n")
    {
        ui->loadTextEdit->insertPlainText (tr("Connected !\n"));
        usart_temp.clear ();
        ui->loadButton->setEnabled (true);//??????????????????upload
    }
    if(usart_temp == "Next\n")
    {
        emit transmitData(binLoadCnt++);//????????????????????????
        //ui->loadTextEdit->insertPlainText (tr("writing...%1K\n").arg (binLoadCnt-1));
        ui->loadTextEdit->insertPlainText (tr("."));//??????????????????????????????????????????
        usart_temp.clear ();
    }
    if(usart_temp == "Ok\n")
    {
        //ui->loadTextEdit->insertPlainText (tr("writing...%1K\n").arg (binLoadCnt));
        ui->loadTextEdit->insertPlainText (tr("\nFinish.\n"));
        ui->loadTextEdit->insertPlainText (tr("Upgrade Complete !\n"));
        usart_temp.clear ();
    }
    if(usart_temp == "Again\n")
    {
        emit transmitData (--binLoadCnt);
        //ui->loadTextEdit->insertPlainText (tr("Failed to write, try again\n"));
        ui->loadTextEdit->insertPlainText (tr("*"));//??????????????????????????????*
        usart_temp.clear ();
    }
    if(usart_temp == "Limit\n")
    {
        emit transmitData (--binLoadCnt);
        ui->loadTextEdit->insertPlainText (tr("\nFlash size limited !\n"));
        usart_temp.clear ();
    }
}

void Widget::transmitDataFun (int cnt) //????????????
{
    qint32 temp = 0; //?????????????????? //??????????????????qint16??????????????????????????????????????????QIODevice::read: Called with maxSize < 0

    ui->loadButton->setEnabled (false);
    QFile *binFile = new QFile(fileLocation);
    binFile->open (QIODevice::ReadOnly);
    binFile->seek (cnt * 1024);

    char * binByte = new char[1024]; //bin??????
    memset (binByte, 0xff, 1024);

    //data_len_L  data_len_H  data(no more than 1K)   index_L index_H CRC

    QByteArray binTransmit;
    binTransmit.resize (8);
    temp = binSize - 1024*cnt;
    if(temp < 1024)
    {
        binTransmit[0] = temp % 256;
        binTransmit[1] = temp / 256;
        //index CRC ??????
        binTransmit[2] = cnt % 256;
        binTransmit[3] = cnt / 256;

        binTransmit[4] = 0xff;
        binTransmit[5] = 0xff;
        binTransmit[6] = 0xff;
        binTransmit[7] = 0xff;

        binFile->read (binByte, temp);
        binTransmit.insert(2, binByte, 1024);

        crc.crc32 (&binTransmit, binTransmit.size () - 4);
        unsigned long crc_val = crc.getCrc32 ();

        binTransmit[1024 + 4] = crc_val;
        binTransmit[1024 + 5] = crc_val >> 8;
        binTransmit[1024 + 6] = crc_val >> 16;
        binTransmit[1024 + 7] = crc_val >> 24;

        ui->loadProgressBar->setValue (100);
    }
    else
    {
        binTransmit[0] = 0x00;
        binTransmit[1] = 0x04;
        //index CRC ??????
        binTransmit[2] = cnt % 256;     //L
        binTransmit[3] = cnt / 256;     //H
        binTransmit[4] = 0xff;          //L
        binTransmit[5] = 0xff;
        binTransmit[6] = 0xff;
        binTransmit[7] = 0xff;          //H

        binFile->read (binByte, 1024);
        binTransmit.insert(2, binByte, 1024);

        crc.crc32 (&binTransmit, binTransmit.size () - 4);
        unsigned long crc_val = crc.getCrc32 ();

        binTransmit[1024 + 4] = crc_val;
        binTransmit[1024 + 5] = crc_val >> 8;
        binTransmit[1024 + 6] = crc_val >> 16;
        binTransmit[1024 + 7] = crc_val >> 24;


        int i = (1 - float(temp) / float(binSize)) * 100;
        ui->loadProgressBar->setValue (i);
    }
    delete binByte;
    serial->write(binTransmit);
    serial->write ("\n");       //???????????????????????????????????????STM32????????????
}

void Widget::on_openButton_clicked()
{
    char reset_command[2] = {0x19,'\0'}; //????????????????????????????????????????????????????????????????????????????????????????????????

    if(!OpenStatus)
    {
        currentSettings.name = ui->uNameBox->currentText ();
        currentSettings.baudRate = 115200;
        currentSettings.dataBits = QSerialPort::Data8;
        currentSettings.stopBits = QSerialPort::OneStop;
        currentSettings.parity = QSerialPort::NoParity;

        serial->setPortName (currentSettings.name);
        if(serial->open (QIODevice::ReadWrite))
        {
            if( serial->setBaudRate (currentSettings.baudRate)
                && serial->setDataBits (currentSettings.dataBits)
                && serial->setStopBits (currentSettings.stopBits)
                && serial->setParity (currentSettings.parity) )
            {
                OpenStatus = true;
                //ui->loadButton->setEnabled (true);//????????????????????????????????????load??????????????????????????????????????????
                ui->openButton->setText (tr("Close"));

                ui->loadTextEdit->insertPlainText (tr("Detecting...\n"));//?????????????????????????????????...

                serial->write (reset_command);//??????????????????0x19????????????
            }
            else
            {
                serial->close ();
                ui->loadButton->setEnabled (false);
                OpenStatus = false;
                ui->openButton->setText (tr("Open"));
            }
        }
        else {
            OpenStatus = false;
            ui->loadButton->setEnabled (false);
            ui->openButton->setText (tr("Open"));
        }
    }
    else
    {
        OpenStatus = false;
        ui->loadButton->setEnabled (false);
        ui->openButton->setText (tr("Open"));
        serial->close ();
    }
}

void Widget::on_fileButton_clicked() //????????????
{
    //???????????????????????????????????????????????????????????????xxx.upgrade???
    fileLocation = QFileDialog::getOpenFileName (this, tr("Open File"),fileLocation, tr("File (*.upgrade)"));
    ui->fileEdit->setText (fileLocation);
    if(fileLocation != NULL)
    {
        QFileInfo *temp = new QFileInfo(fileLocation);
        binSize = temp->size ();
        if(binSize < 1024)
            ui->sizeLabel->setText (tr("Size: %1 B").arg (binSize));
        else if(binSize < 1024 * 1024)
            ui->sizeLabel->setText (tr("Size: %1 K %2 B").arg (binSize / 1024).arg (binSize % 1024));
        else
            ui->sizeLabel->setText (tr("Size: %1 M %2 K").arg (binSize / 1024 / 1024)
                                    .arg (binSize / 1024 %1024));

        QFile *binFile = new QFile(fileLocation);
        if(!binFile->open (QIODevice::ReadOnly))emit binFileOpenFailed ();  //??????????????????
        else
        {
            ui->loadButton->setEnabled (true);
            binFile->close ();
        }
        delete binFile;
        delete temp;
    }
}

void Widget::on_loadButton_clicked()
{
    ui->loadTextEdit->insertPlainText (tr("Downloading."));
    binLoadCnt = 1;          //????????? ??????????????? binLoadCnt*1024
    usart_temp.clear ();
    emit transmitData (0);   //?????????K??????
}

void Widget::on_refreshButton_clicked()
{
    fillPortsCom();
}

void Widget::on_loadTextEdit_textChanged()
{
    ui->loadTextEdit->moveCursor(QTextCursor::End);//????????????????????????
}
