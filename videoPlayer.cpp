#include <stdio.h>

extern "C" {
#include "libavformat/avformat.h"	//��װ��ʽ
#include "libavcodec/avcodec.h"	//����
#include "libswscale/swscale.h"	//����
#include "libavutil/imgutils.h" //����ͼ��ʹ��
#include "SDL2/SDL.h"	//����
};


#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)


//flag
int thread_exit = 0;
//int thread_pause = 0;

//������һ��SDL�̣߳�ÿ���̶�ʱ�䣨=ˢ�¼��������һ���Զ������Ϣ����֪���������н�����ʾ��ʹ����ˢ�¼��������40����
int sfp_refresh_thread(void* opaque) {
	int fps = (int)opaque; //ÿ�����֡������ˢ�¼����(1000/frame_per_sec)���룬Ĭ��25
	while (!thread_exit) {
		SDL_Event event;
		event.type = SFM_REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(1000/fps);
	}
	thread_exit = 0;

	SDL_Event event;
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);

	return 0;
}


int playVideo(char* filepath, int fps) {
	//��ȡ�ļ�·��
	//char filepath[] = "test.mp4";		

	AVFormatContext* pFormatCtx; //���װ
	AVCodecContext* pCodecCtx; //����
	AVCodec* pCodec;
	AVFrame* pFrame, * pFrameYUV; //֡����
	AVPacket* packet;	//����ǰ��ѹ�����ݣ������ݣ�
	int index;
	unsigned char* out_buffer;	//���ݻ�����
	struct SwsContext* img_convert_ctx;

	int screen_w = 0, screen_h = 0;
	SDL_Window* screen; //SDL�����Ĵ���
	SDL_Renderer* sdlRenderer; //��Ⱦ������ȾSDL_Texture��SDL_Window
	SDL_Texture* sdlTexture; //����
	SDL_Rect sdlRect;
	SDL_Thread* video_tid; //���̣߳�ͬ��ˢ��ʱ��
	SDL_Event event; //�߳�״̬

	av_register_all();	//ע���
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	//��ʼ��SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))	{
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	//����Ƶ�ļ�����ʼ��pFormatCtx
	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		return -1;
	}
	//��ȡ�ļ���Ϣ
	if (avformat_find_stream_info(pFormatCtx, NULL)<0) {
		printf("Couldn't find stream information.\n");
		return -1;
	}
	//��ȡ����ý�����ı�������Ϣ���ҵ���Ӧ��type���ڵ�pFormatCtx->streams������λ�ã���ʼ��������
	index = -1;
	for (int i = 0; i<pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			index = i;
			break;
		}
	if (index == -1) {
		printf("Didn't find a video stream.\n");
		return -1;
	}
	//��ȡ������
	pCodecCtx = pFormatCtx->streams[index]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		printf("Codec not found.\n");
		return -1;
	}
	//�򿪽�����
	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
		printf("Could not open codec.\n");
		return -1;
	}

	//�ڴ����
	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();
	packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	//�������ڴ����
	out_buffer = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
	//�������󶨵������AVFrame��
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
		AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);
	//��ʼ��img_convert_ctx
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);


	//�����Ƶ�ļ���Ϣ
	//printf("---------------- File Information ---------------\n");
	//av_dump_format(pFormatCtx, 0, filepath, 0);
	//printf("-------------------------------------------------\n");

	//����SDL����
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow(filepath, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h, SDL_WINDOW_OPENGL);

	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	int ret, got_picture;
	video_tid = SDL_CreateThread(sfp_refresh_thread, NULL, (void*)fps);

	while (1) {
		SDL_WaitEvent(&event);
		if (event.type==SFM_REFRESH_EVENT) {	//����40msˢ��һ��
			while (1) {
				if (av_read_frame(pFormatCtx, packet) < 0) //���װý���ļ�
					thread_exit = 1;
				if (packet->stream_index == index)
					break;
			}
			// ����packet��pFrame
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);	
			if (ret < 0) {
				printf("Decode Error.\n");
				return -1;
			}
			if (got_picture) {
				//�������䣬�������������ݴ���pFrameYUV->data��
				sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
					pFrameYUV->data, pFrameYUV->linesize);
				//SDL��ʾ��Ƶ
				SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
				SDL_RenderClear(sdlRenderer);
				SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
				SDL_RenderPresent(sdlRenderer);
			}
			av_free_packet(packet);
		}
		else if (event.type==SDL_QUIT) {
			thread_exit = 1;
		}
		else if (event.type==SFM_BREAK_EVENT) {
			break;
		}
	}
	sws_freeContext(img_convert_ctx);
	av_free(out_buffer);
	av_free_packet(packet);
	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	SDL_Quit();
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	return 0;
}
