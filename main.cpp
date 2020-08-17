#include <iostream>
#include <cstdlib>

int playVideo(char* filepath, int fps);

int main(int argc, char* argv[]) {
	char* filepath;
	int fps = 25; // ÿ�����֡������ȷ������ˢ�¼����Ĭ��ֵ25
	if (argc==2) {
		filepath = argv[1];
	}
	else if (argc==3) {
		filepath = argv[1];
		fps = atoi(argv[2]);
	}
	else {
		printf("Usage: 'player <filename>' or 'player <filename> <fps>'");
		return -1;
	}

	playVideo(filepath, fps);

    system("pause");

    return 0;
}