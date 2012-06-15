#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDebug>
#include <QTime>
#include <QEventLoop>
#include <OpenCL/cl.h>
#include "qjson/qjsondocument.h"
#include "qjson/qjsonobject.h"
#include "qjson/qjsonvalue.h"

#define MAX_NONCE 1048576
#define OPENCL_WORKER

#include "SHA256.h"

#define BIGENDIAN_READ(a, d) \
    ((((uchar) a.at(d)) << 24L) + (((uchar) a.at(d + 1)) << 16L) + (((uchar) a.at(d + 2)) << 8L) + (((uchar) a.at(d + 3))))

QByteArray jsonRequest(const QByteArray &in)
{
    QString userName = "wishstudio.lovebit";
    QString password = "LBIT";
    QNetworkAccessManager *manager = new QNetworkAccessManager();
    QNetworkRequest request;
    request.setUrl(QUrl("http://api.bitcoin.cz:8332"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    request.setRawHeader("Authorization", "Basic " + QByteArray(QString("%1:%2").arg(userName).arg(password).toAscii()).toBase64());

    QNetworkReply *reply = manager->post(request, in);

    QEventLoop loop;
    QObject::connect(manager, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()));
    loop.exec();
    return reply->readAll();
}

void die(QString message)
{
    qDebug() << message;
    exit(1);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
#ifdef OPENCL_WORKER
    cl_int err;
    cl_uint platform_count;
    err = clGetPlatformIDs(0, NULL, &platform_count);
    if (err != CL_SUCCESS)
        die("clGetPlatformIDs() failed.");
    if (platform_count <= 0)
        die("No cl platform found.");
    cl_platform_id *platforms = new cl_platform_id[platform_count];
    err = clGetPlatformIDs(platform_count, platforms, 0);
    if (err != CL_SUCCESS)
        die("clGetPlatformIDs() failed.");

    cl_uint device_count;
    err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 0, NULL, &device_count);
    if (err != CL_SUCCESS)
        die("clGetDeviceIDs() failed.");
    if (device_count <= 0)
        die("No cl device found.");
    cl_device_id *devices = new cl_device_id[device_count];
    err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, device_count, devices, NULL);
    if (err != CL_SUCCESS)
        die("clGetDeviceIDs() failed.");

    cl_context context = clCreateContext(NULL, device_count, devices, NULL, NULL, &err);
    if (err != CL_SUCCESS)
        die("clGetContext() failed.");

    cl_command_queue command_queue = clCreateCommandQueue(context, devices[0], CL_QUEUE_PROFILING_ENABLE, &err);
    if (err != CL_SUCCESS)
        die("clGetCommandQueue() failed.");

    QFile file(":/SHA256.cl");
    file.open(QIODevice::ReadOnly);
    QByteArray content = file.readAll();
    file.close();

    const char *prog = content.constData();

    cl_program program = clCreateProgramWithSource(context, 1, (const char **) &prog, NULL, &err);
    if (err != CL_SUCCESS)
        die("clCreateProgramWithSource() failed.");
    err = clBuildProgram(program, 0, NULL, "", NULL, NULL);
    if (err != CL_SUCCESS)
    {
        puts("CL program build failed.");
        char buildlog[16384];
        err = clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, 16384, buildlog, NULL);
        if (err != CL_SUCCESS)
            die("Cannot fetch build log.");
        die(buildlog);
    }
    cl_kernel kernel = clCreateKernel(program, "search", &err);
    if (err != CL_SUCCESS)
        die("clCreateKernel() failed.");

    cl_mem out = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 2 * 4, NULL, &err);
    if (err != CL_SUCCESS)
        die("clCreateBuffer() failed.");
#endif
    bool found = false;
    int nonce;

    QByteArray data, target;
    for (;;)
    {
        QByteArray replyData;
        if (found)
        {
            data[76] = (nonce >> 24) & 0xFF;
            data[77] = (nonce >> 16) & 0xFF;
            data[78] = (nonce >>  8) & 0xFF;
            data[79] = (nonce      ) & 0xFF;
            replyData = jsonRequest("{\"method\":\"getwork\",\"params\":{\"" + data + "\"},\"id\":\"jsonrpc\"}");
            qDebug() << "Share submitted.";
        }
        else
            replyData = jsonRequest("{\"method\":\"getwork\",\"params\":{},\"id\":\"jsonrpc\"}");

        QJsonObject object = QJsonDocument::fromJson(replyData).object().value("result").toObject();
        data = QByteArray::fromHex(object.value("data").toString().toAscii());
        target = QByteArray::fromHex(object.value("target").toString().toAscii());
        uint d[32];

        for (int i = 0; i < 76; i += 4)
            d[i / 4] = BIGENDIAN_READ(data, i);

        /* 80 / 4 = 20 numbers */
        /* padding bit */
        d[20] = 0x80000000;
        d[21] = d[22] = d[23] = d[24] = d[25] = d[26] = d[27] = d[28] = d[29] = d[30] = 0;
        /* length, 80 * 8 = 640 bits */
        d[31] = 640;

        uint tg[8];
        for (int i = 0; i < 32; i += 4)
            tg[i / 4] = BIGENDIAN_READ(target, 32 - i - 4);

        uint w[64];
        memcpy(w, d, sizeof(int) * 16);

        SHA256 f;
        f.update(w);

        QTime time;
        time.start();
#ifdef OPENCL_WORKER
        clSetKernelArg(kernel, 0, sizeof(uint), (void *) &f.h0);
        clSetKernelArg(kernel, 1, sizeof(uint), (void *) &f.h1);
        clSetKernelArg(kernel, 2, sizeof(uint), (void *) &f.h2);
        clSetKernelArg(kernel, 3, sizeof(uint), (void *) &f.h3);
        clSetKernelArg(kernel, 4, sizeof(uint), (void *) &f.h4);
        clSetKernelArg(kernel, 5, sizeof(uint), (void *) &f.h5);
        clSetKernelArg(kernel, 6, sizeof(uint), (void *) &f.h6);
        clSetKernelArg(kernel, 7, sizeof(uint), (void *) &f.h7);

        clSetKernelArg(kernel, 8, sizeof(uint), (void *) &d[16]);
        clSetKernelArg(kernel, 9, sizeof(uint), (void *) &d[17]);
        clSetKernelArg(kernel, 10, sizeof(uint), (void *) &d[18]);

        clSetKernelArg(kernel, 11, sizeof(cl_mem), (void *) &out);

        uint ret[2];
        size_t global_size[] = {MAX_NONCE};
        err = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, global_size, NULL, 0, NULL, NULL);
        if (err != CL_SUCCESS)
            die("clEnqueueNDRangeKernel() failed.");
        err = clFlush(command_queue);
        if (err != CL_SUCCESS)
            die("clFlush() failed.");
        err = clFinish(command_queue);
        if (err != CL_SUCCESS)
            die("clFinish() failed");
        err = clEnqueueReadBuffer(command_queue, out, true, 0, 2 * 4, (void *) ret, 0, NULL, NULL);
        if (err != CL_SUCCESS)
            die("clEnqueueReadBuffer() failed.");

        found = ret[0] == 1;
        nonce = ret[1];
        qDebug() << found << nonce;
#else
        for (nonce = 0; nonce < MAX_NONCE; nonce++)
        {
            SHA256 ff;
            ff.h0 = f.h0;
            ff.h1 = f.h1;
            ff.h2 = f.h2;
            ff.h3 = f.h3;
            ff.h4 = f.h4;
            ff.h5 = f.h5;
            ff.h6 = f.h6;
            ff.h7 = f.h7;
            memcpy(w, d + 16, sizeof(int) * 16);

            /* w[3] = d[19] */
            w[3] = nonce;
            ff.update(w);

            w[0] = ff.h0;
            w[1] = ff.h1;
            w[2] = ff.h2;
            w[3] = ff.h3;
            w[4] = ff.h4;
            w[5] = ff.h5;
            w[6] = ff.h6;
            w[7] = ff.h7;

            /* padding bit */
            w[8] = 0x80000000;
            w[9] = w[10] = w[11] = w[12] = w[13] = w[14] = 0;
            /* length, 256 bits */
            w[15] = 256;

            SHA256 final;
            final.update(w);

            w[0] = final.h0;
            w[1] = final.h1;
            w[2] = final.h2;
            w[3] = final.h3;
            w[4] = final.h4;
            w[5] = final.h5;
            w[6] = final.h6;
            w[7] = final.h7;
            for (int i = 0; i < 8; i++)
                if (w[i] < tg[i])
                {
                    found = true;
                    break;
                }
                else if (w[i] > tg[i])
                    break;
            if (found)
                break;
        }
#endif
        if (time.elapsed())
            qDebug() << "Hashrate: " << ((double) MAX_NONCE / time.elapsed() / 1000) << "Mhash/s";
    }
    return 0;
}
