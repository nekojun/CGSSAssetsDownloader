#pragma once

#include <iostream>  
#include <sstream>
#include <math.h>
#include <time.h>
#include <WinSock2.h>  
using namespace std;
#pragma comment(lib, "ws2_32.lib")  

long get_file_size(const char* strFileName) {
	struct _stat info;
	_stat(strFileName, &info);
	long size = info.st_size;
	return size;
}

string Int_to_String(int n) {
	ostringstream stream;
	stream << n; 
	return stream.str();
}

void progress(double local, double current, double max, double speed) {
	double p_local = round(local / max * 50);
	double p_current = round(current / max * 50);
	double percent = (local + current) / max * 100;
	printf("\r");
	printf("%.2lf / %.2lf MB ", (local + current) / 1024 / 1024, max / 1024 / 1024);
	printf("[");
	for (int i = 0; i < (int)p_local; i++) {
		printf("+");
	}
	for (int i = 0; i < (int)p_current/* - 1*/; i++) {
		printf("=");
	}
	printf(">");
	for (int i = 0; i < (int)(50 - p_local - p_current); i++) {
		printf(" ");
	}
	printf("] ");
	printf("%5.2f%% ", percent);
	printf("%7.2lf KB/s", speed / 1024);
	printf("   \b\b\b");
}

void progress(double current, double max) {
	double p_current = floor(current / max * 50);
	double percent = current / max * 100;
	printf("Completed: %.0lf / %.0lf ", current, max);
	printf("[");
	for (int i = 0; i < (int)p_current - 1; i++) {
		printf("=");
	}
	printf(">");
	for (int i = 0; i < (int)(50 - p_current); i++) {
		printf(" ");
	}
	printf("] ");
	printf("%5.2f%% ", percent);
	printf("   \b\b\b");
}

const int BuffSize = 1024; //设置缓冲区大小

void download(string url, string path) {
	// 获取主机名
	size_t protocol = url.find("//") + 2;
	string hostname = url.substr(protocol, url.substr(protocol).find_first_of('/'));
	// 获取路由
	string route = url.substr(protocol).substr(url.substr(protocol).find_first_of('/'));

	// 获取目标文件大小，判断目标文件是否已存在
	long size = get_file_size(path.c_str());

	WSADATA WsaData;
	if (WSAStartup(MAKEWORD(2, 2), &WsaData)) {
		printf("Init failed.\n");
		return;
	}

	SOCKET sockeId;
	SOCKADDR_IN addr;
	hostent *remoteHost;
	remoteHost = gethostbyname(hostname.c_str());

	if (-1 == (sockeId = socket(AF_INET, SOCK_STREAM, 0))) {
		printf("Create socket failed.\n");
		return;
	}

	addr.sin_addr.S_un.S_addr = *((unsigned long *)*remoteHost->h_addr_list);   //inet_addr("104.116.243.18")
	addr.sin_family = AF_INET;
	addr.sin_port = htons(80);

	if (SOCKET_ERROR == connect(sockeId, (SOCKADDR *)&addr, sizeof(addr))) {
		printf("Connect server failed.\n");
		closesocket(sockeId);
		WSACleanup();
		return;
	}

	// 初始化请求的数据  
	char* pReqHead = new char[BuffSize];
	pReqHead[0] = '\0';
	// 请求行  
	strcat(pReqHead, "GET ");
	strcat(pReqHead, route.c_str());
	strcat(pReqHead, " HTTP/1.1\r\n");

	// 请求头  
	string hosthead = "Host: " + hostname + "\r\n";

	strcat(pReqHead, hosthead.c_str());
	strcat(pReqHead, "Accept: */*\r\n");
	strcat(pReqHead, "Connection: Keep-Alive\r\n");
	strcat(pReqHead, "User-Agent: Dalvik/2.1.0 (Linux; U; Android 7.0; Nexus 42 Build/XYZZ1Y)\r\n");
	strcat(pReqHead, "X-Unity-Version: 5.1.2f1\r\n");
	strcat(pReqHead, "Accept-Encoding: gzip\r\n");
	if (size != 0) {
		// 获取已下载的字节数，断点续传
		string range = "Range: bytes=" + Int_to_String(size) + "-\r\n";
		strcat(pReqHead, range.c_str());
	}
	strcat(pReqHead, "\r\n");
	//printf(pReqHead);

	if (SOCKET_ERROR == send(sockeId, pReqHead, strlen(pReqHead), 0)) {
		printf("Send data failed.\n");
		closesocket(sockeId);
		WSACleanup();
		delete pReqHead;
		return;
	}

	delete pReqHead;

	FILE *fp;
	fp = fopen(path.c_str(), "ab+");

	if (NULL == fp) {
		printf("Create file failed.\n");
		closesocket(sockeId);
		WSACleanup();
		return;
	}

	char* buff = (char*)malloc(BuffSize * sizeof(char));
	memset(buff, '\0', BuffSize);
	int iRec = 1; // 接收字节
	bool bStart = false; //是否读完返回头
	int chars = 0; // 用于判断返回头末尾

	//接收返回头
	string str;
	while (!bStart) {
		iRec = recv(sockeId, buff, 1, 0); //一个字节一个字节的接受HTTP头
		str += *buff;
		if (iRec < 0) {
			bStart = true;
		}
		switch (*buff)
		{
		case '\r':
			break;
		case '\n'://判断HTTP头是否接受完毕
			if (chars == 0) {
				bStart = true;
			}
			chars = 0;
			break;
		default:
			chars++;
			break;
		}
	}

	//printf("%s\n", str.c_str());

	// 获取返回头中的Content-Length
	int length = -1;
	if (str.find("Content-Length: ") != str.npos) {
		string contentlength1 = str.substr(str.find("Content-Length: "));
		string contentlength2 = contentlength1.substr(0, contentlength1.find_first_of('\r'));
		string contentlength3 = contentlength2.substr(16);
		length = atoi(contentlength3.c_str());
	}
	else {
		printf("Download faild.\n");
		return;
	}

	if (size == length && str.find("Content-Range") == str.npos) {
		printf("File exists.\n");
	}
	else {
		// 开始接收
		int sum = 0;
		int speed = 0;
		double start_time = clock();
		double end_time = 0;
		int last_time = 0;
		do {
			iRec = recv(sockeId, buff, BuffSize, 0);
			if (iRec < 0) {
				break;
			}
			sum += iRec;
			speed += iRec;
			//printf("\r%d", sum);
			int now = clock();
			if (now - last_time > 500) {
				progress(size, sum, length + size, speed * 2);
				last_time = now;
				speed = 0;
			}

			fwrite(buff, iRec, 1, fp);
			if (sum == length) {
				end_time = clock();
				progress(size, sum, length + size, (double)sum / ((end_time - start_time) / 1000));
				closesocket(sockeId);
				WSACleanup();
				break;
			}
		} while (iRec > 0);
	}

	fclose(fp);
	free(buff);
	printf("\n\n");
}
