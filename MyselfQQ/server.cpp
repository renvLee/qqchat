#include "server.h"
#include "ui_server.h"

#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>

Server::Server(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Server)
{
    ui->setupUi(this);
    setFixedSize(400,207);

    tPort = 5555;
    tSrv = new QTcpServer(this);
    connect(tSrv, SIGNAL(newConnection()), this, SLOT(sndMsg()));

    initSrv();
}

Server::~Server()
{
    delete ui;
}

void Server::initSrv()
{
    payloadSize = 64*1024;
    totalBytes = 0;
    bytesWritten = 0;
    bytesTobeWrite = 0;

    ui->sStatusLbl->setText(tr("请选择要传送的文件"));
    ui->progressBar->reset();
    ui->sOpenBtn->setEnabled(true);
    ui->sSendBtn->setEnabled(false);

    tSrv->close();
}
/*
 * @brief   :发送数据
 * @author  :Lengde
 * @date    :2024.03.17
 */
void Server::sndMsg()
{
    ui->sSendBtn->setEnabled(false);
    clntConn = tSrv->nextPendingConnection();
    connect(clntConn,SIGNAL(bytesWritten(qint64)),this,SLOT(updClntProgress(qint64)));

    ui->sStatusLbl->setText(tr("开始传送文件 %1 ！").arg(theFileName));

    locFile = new QFile(fileName);
    if(!locFile->open((QFile::ReadOnly))){//以只读方式打开文件
        QMessageBox::warning(this, tr("应用程序"), tr("无法读取文件 %1:\n%2").arg(fileName).arg(locFile->errorString()));
        return;
    }
    //获取文件的大小
    totalBytes = locFile->size();
    //将发送缓冲区封装outBlock封装在一个QDataStream类型的变量中，可以方便地通过“<<"操作符填写文件头结构
    QDataStream sendOut(&outBlock, QIODevice::WriteOnly);
    sendOut.setVersion(QDataStream::Qt_5_8);
    time.start();  // 开始计时
    //通过right()去掉文件的路路径部分，仅将文件部分保存在curFile变量中
    QString curFile = fileName.right(fileName.size() - fileName.lastIndexOf('/')-1);
    //构造一个临时的文件头，将该值追加到totalBytes字段，从而完成实际需要发送字节数的记录
    sendOut << qint64(0) << qint64(0) << curFile;
    totalBytes += outBlock.size();
    //将读写操作指向从头开始
    sendOut.device()->seek(0);
    //填写实际的总长度和文件长度
    sendOut << totalBytes << qint64((outBlock.size() - sizeof(qint64)*2));
    //将文件头发出，同时修改待发送字节数bytesTobeWrite
    bytesTobeWrite = totalBytes - clntConn->write(outBlock);
    //清空发送缓冲区，以备下次使用
    outBlock.resize(0);
}

void Server::updClntProgress(qint64 numBytes)
{
    qApp->processEvents();
    bytesWritten += (int)numBytes;
    if (bytesTobeWrite > 0) {
        outBlock = locFile->read(qMin(bytesTobeWrite, payloadSize));
        bytesTobeWrite -= (int)clntConn->write(outBlock);
        outBlock.resize(0);
    } else {
        locFile->close();
    }
    ui->progressBar->setMaximum(totalBytes);
    ui->progressBar->setValue(bytesWritten);

    float useTime = time.elapsed();
    double speed = bytesWritten / useTime;
    ui->sStatusLbl->setText(tr("已发送 %1MB (%2MB/s) \n共%3MB 已用时:%4秒\n估计剩余时间：%5秒")
                   .arg(bytesWritten / (1024*1024))
                   .arg(speed*1000 / (1024*1024), 0, 'f', 2)
                   .arg(totalBytes / (1024 * 1024))
                   .arg(useTime/1000, 0, 'f', 0)
                   .arg(totalBytes/speed/1000 - useTime/1000, 0, 'f', 0));

    if(bytesWritten == totalBytes) {
        locFile->close();
        tSrv->close();
        ui->sStatusLbl->setText(tr("传送文件 %1 成功").arg(theFileName));
    }
}
void Server::on_sOpenBtn_clicked()
{
    fileName = QFileDialog::getOpenFileName(this);
    if(!fileName.isEmpty())
    {
        theFileName = fileName.right(fileName.size() - fileName.lastIndexOf('/')-1);
        ui->sStatusLbl->setText(tr("要传送的文件为：%1 ").arg(theFileName));
        ui->sSendBtn->setEnabled(true);
        ui->sOpenBtn->setEnabled(false);
    }
}

void Server::on_sSendBtn_clicked()
{
    if(!tSrv->listen(QHostAddress::Any,tPort))//开始监听
    {
        qDebug() << tSrv->errorString();
        close();
        return;
    }

    ui->sStatusLbl->setText(tr("等待对方接收... ..."));
    emit sndFileName(theFileName);
}

void Server::on_sCloseBtn_clicked()
{
    if(tSrv->isListening())
    {
        tSrv->close();
        if (locFile->isOpen())
            locFile->close();
        clntConn->abort();
    }
    close();
}

void Server::refused()
{
    tSrv->close();
    ui->sStatusLbl->setText(tr("对方拒绝接收！"));
}

void Server::closeEvent(QCloseEvent *)
{
    on_sCloseBtn_clicked();
}
