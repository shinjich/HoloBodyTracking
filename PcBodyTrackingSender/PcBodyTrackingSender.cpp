#ifndef STRICT
#define STRICT
#endif
#include "udp.h"
#include "BodyStruct.h"
#include "BitSaving.h"
#include <tchar.h>

// Win32 �A�v���p
static const TCHAR szClassName[] = TEXT("OnSiteBodyTracking4PC");
HWND g_hWnd = NULL;							// �A�v���P�[�V�����̃E�B���h�E
HBITMAP g_hBMP = NULL, g_hBMPold = NULL;	// �\������r�b�g�}�b�v�̃n���h��
HDC g_hDCBMP = NULL;						// �\������r�b�g�}�b�v�̃R���e�L�X�g
BITMAPINFO g_biBMP = { 0, };				// �r�b�g�}�b�v�̏�� (�𑜓x��t�H�[�}�b�g)
LPDWORD g_pdwPixel = NULL;					// �r�b�g�}�b�v�̒��g�̐擪 (�s�N�Z�����)
UINT16* g_pDepthMap = NULL;					// �[�x�}�b�v�o�b�t�@�̃|�C���^

// �{�f�B�`��p
HPEN g_hPen = NULL;
k4abt_skeleton_t g_Skeleton[MAX_BODIES];
k4a_float2_t g_fSkeleton2D[MAX_BODIES][K4ABT_JOINT_COUNT] = { 0.0f, };
uint32_t g_uBodyID[MAX_BODIES] = { K4ABT_INVALID_BODY_ID, };
uint32_t g_uBodies = 0;

// Azure Kinect �p
k4a_device_t g_hAzureKinect = nullptr;

WS2* g_pWs = nullptr;
RBT_STRUCT_SAVED* g_pImageRBT = nullptr;
BITSAVING g_bsave;

void DestroyKinectOnSite()
{
	if ( g_hAzureKinect )
	{
		// Azure Kinect ���~����
		k4a_device_stop_cameras( g_hAzureKinect );

		// Azure Kinect �̎g�p����߂�
		k4a_device_close( g_hAzureKinect );
		g_hAzureKinect = nullptr;
	}
}

HRESULT CreateKinectOnSite()
{
	// Azure Kinect ������������
	k4a_result_t hr = k4a_device_open( K4A_DEVICE_DEFAULT, &g_hAzureKinect );
	if ( hr == K4A_RESULT_SUCCEEDED )
	{
		// Azure Kinect �̃J�����ݒ�
		k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
		config.depth_mode = K4A_DEPTH_MODE;

		// Azure Kinect �̎g�p���J�n����
		hr = k4a_device_start_cameras( g_hAzureKinect, &config );
		if ( hr == K4A_RESULT_FAILED )
		{
			MessageBox( NULL, TEXT("Azure Kinect ���J�n�ł��܂���ł���"), TEXT("�G���["), MB_OK );
			// Azure Kinect �̎g�p����߂�
			k4a_device_close( g_hAzureKinect );
			return E_FAIL;
		}
	}

	return S_OK;
}

uint32_t KinectProcOnSite()
{
	HRESULT hResult = E_FAIL;

	// �L���v�`���[����
	k4a_capture_t hCapture = nullptr;
	k4a_wait_result_t hr = k4a_device_get_capture( g_hAzureKinect, &hCapture, K4A_WAIT_INFINITE );
	if ( hr == K4A_WAIT_RESULT_SUCCEEDED )
	{
		uint32_t uImageSize = 0;
		k4a_image_t hSrcDepth;
		k4a_image_t hSrcIR;

		// �[�x�C���[�W���擾����
		hSrcDepth = k4a_capture_get_depth_image( hCapture );
		hSrcIR = k4a_capture_get_ir_image( hCapture );

		// �L���v�`���[���������
		k4a_capture_release( hCapture );

		if ( hSrcDepth )
		{
			// �C���[�W�s�N�Z���̐擪�|�C���^���擾����
			uint8_t* p = k4a_image_get_buffer( hSrcDepth );
			if ( p )
			{
				// �C���[�W�T�C�Y���擾����
				uImageSize = (uint32_t) k4a_image_get_size( hSrcDepth );
				for( size_t i = 0; i < PACKET_DIVIDE; i++ )
				{
					g_pImageRBT[i].dwHeader = (DWORD) i;
					g_bsave.Save( g_pImageRBT[i].cDepth, &p[i * (RESOLUTION_SIZE / PACKET_DIVIDE) * sizeof(UINT16)], uImageSize / sizeof(UINT16) / PACKET_DIVIDE, SCOPE_DISTANCE_BITS_D );
				}
				CopyMemory( g_pDepthMap, p, uImageSize );
			}
			// �C���[�W���������
			k4a_image_release( hSrcDepth );
		}
		if ( hSrcIR )
		{
			// �C���[�W�s�N�Z���̐擪�|�C���^���擾����
			uint8_t* p = k4a_image_get_buffer( hSrcIR );
			if ( p )
			{
				// �C���[�W�T�C�Y���擾����
				uImageSize = (uint32_t) k4a_image_get_size( hSrcIR );
				for( size_t i = 0; i < PACKET_DIVIDE; i++ )
				{
					g_bsave.Save( g_pImageRBT[i].cIr, &p[i * (RESOLUTION_SIZE / PACKET_DIVIDE) * sizeof(UINT16)], uImageSize / sizeof(UINT16) / PACKET_DIVIDE, SCOPE_DISTANCE_BITS_I );
				}
			}
			// �C���[�W���������
			k4a_image_release( hSrcIR );
		}

		if ( hSrcDepth && hSrcIR )
		{
			for( size_t i = 0; i < PACKET_DIVIDE; i++ )
			{
				g_pWs->UdpSendData( (char*) &g_pImageRBT[i], sizeof(RBT_STRUCT_SAVED) );
				Sleep( 1 );
			}
			hResult = S_OK;
		}
	}

	{
		char sbtbuf[1400];
		UINT32 uBodies = 0;
		for( ; ; )
		{
			int iReceived = g_pWs->UdpReceiveData( sbtbuf, 1400 );
			if ( iReceived > 0 )
			{
				uBodies = ((SBT_STRUCT*) sbtbuf)->dwHeader;
				CopyMemory( &g_Skeleton[0], &((SBT_STRUCT*) sbtbuf)->skeleton[0], uBodies * sizeof(k4abt_skeleton_t) );
				CopyMemory( &g_fSkeleton2D[0][0], &((SBT_STRUCT*) sbtbuf)->skeleton2D[0][0], uBodies * K4ABT_JOINT_COUNT * sizeof(k4a_float2_t) );
				CopyMemory( &g_uBodyID[0], &((SBT_STRUCT*) sbtbuf)->bodyID[0], uBodies * sizeof(uint32_t) );
			}
			else if ( iReceived == 0 )
			{
				uBodies = 0;
				break;
			}
			else if ( iReceived < 0 )
			{
				g_uBodies = uBodies;
				break;
			}
		}
	}
	return hResult;
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch( uMsg )
	{
	case WM_PAINT:
		{
			// ��ʕ\������
			TCHAR szText[128];
			PAINTSTRUCT ps;
			HDC hDC = BeginPaint( hWnd, &ps );

			// ��ʃT�C�Y���擾����
			RECT rect;
			GetClientRect( hWnd, &rect );

			// 16-bit �[�x�}�b�v���� 32-bit �J���[�r�b�g�}�b�v�ɕϊ�����
			for( uint32_t u = 0; u < RESOLUTION_SIZE; u++ )
			{
				const WORD w = g_pDepthMap[u];
				g_pdwPixel[u] = 0xFF000000 | (w << 16) | (w << 8) | w;
			}

			// �J���[�������[�x�̕\��
			StretchBlt( hDC, 0, 0, rect.right, rect.bottom, g_hDCBMP, 0, 0, RESOLUTION_WIDTH, RESOLUTION_HEIGHT, SRCCOPY );

			// �[�x�C���[�W�ɑ΂���X�N���[���T�C�Y�̔���v�Z
			const FLOAT fRelativeWidth = (FLOAT) rect.right / (FLOAT) RESOLUTION_WIDTH;
			const FLOAT fRelativeHeight = (FLOAT) rect.bottom / (FLOAT) RESOLUTION_HEIGHT;

			// �`��p�̃y����I��
			HPEN hPenPrev = (HPEN) SelectObject( hDC, g_hPen );
			SetBkMode( hDC, TRANSPARENT );

			for( uint32_t uBody = 0; uBody < g_uBodies; uBody++ )
			{
				// �X�N���[���T�C�Y�ɍ��i���X�P�[�����O
				LONG lSkeletonX[K4ABT_JOINT_COUNT];
				LONG lSkeletonY[K4ABT_JOINT_COUNT];
				for( int iJoint = K4ABT_JOINT_PELVIS; iJoint < K4ABT_JOINT_COUNT; iJoint++ )
				{
					lSkeletonX[iJoint] = (LONG) (g_fSkeleton2D[uBody][iJoint].xy.x * fRelativeWidth);
					lSkeletonY[iJoint] = (LONG) (g_fSkeleton2D[uBody][iJoint].xy.y * fRelativeHeight);

					// ���W��\��
					_stprintf_s( szText, 128, TEXT("%.0f %.0f %.0f"), g_Skeleton[uBody].joints[iJoint].position.xyz.x, g_Skeleton[uBody].joints[iJoint].position.xyz.y, g_Skeleton[uBody].joints[iJoint].position.xyz.z );
					switch( g_Skeleton[uBody].joints[iJoint].confidence_level )
					{
					case K4ABT_JOINT_CONFIDENCE_NONE:
						SetTextColor( hDC, RGB( 0, 0, 255 ) );
						break;
					case K4ABT_JOINT_CONFIDENCE_LOW:
						SetTextColor( hDC, RGB( 255, 0, 0 ) );
						break;
					case K4ABT_JOINT_CONFIDENCE_MEDIUM:
						SetTextColor( hDC, RGB( 255, 255, 0 ) );
						break;
					case K4ABT_JOINT_CONFIDENCE_HIGH:
						SetTextColor( hDC, RGB( 255, 255, 255 ) );
						break;
					default:
						SetTextColor( hDC, RGB( 128, 128, 128 ) );
						break;
					}
					TextOut( hDC, lSkeletonX[iJoint], lSkeletonY[iJoint], szText, (int) _tcslen( szText ) );

				}
				_stprintf_s( szText, 128, TEXT("ID: %d"), g_uBodyID[uBody] );
				SetTextColor( hDC, RGB( 255, 255, 255 ) );
				TextOut( hDC, lSkeletonX[K4ABT_JOINT_NOSE], lSkeletonY[K4ABT_JOINT_NOSE] - 40, szText, (int) _tcslen( szText ) );
				
				// ��
				MoveToEx( hDC, lSkeletonX[K4ABT_JOINT_EAR_LEFT], lSkeletonY[K4ABT_JOINT_EAR_LEFT], NULL );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_EYE_LEFT], lSkeletonY[K4ABT_JOINT_EYE_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_NOSE], lSkeletonY[K4ABT_JOINT_NOSE] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_EYE_RIGHT], lSkeletonY[K4ABT_JOINT_EYE_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_EAR_RIGHT], lSkeletonY[K4ABT_JOINT_EAR_RIGHT] );

				// ��
				MoveToEx( hDC, lSkeletonX[K4ABT_JOINT_PELVIS], lSkeletonY[K4ABT_JOINT_PELVIS], NULL );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_SPINE_NAVEL], lSkeletonY[K4ABT_JOINT_SPINE_NAVEL] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_SPINE_CHEST], lSkeletonY[K4ABT_JOINT_SPINE_CHEST] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_NECK], lSkeletonY[K4ABT_JOINT_NECK] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_HEAD], lSkeletonY[K4ABT_JOINT_HEAD] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_NOSE], lSkeletonY[K4ABT_JOINT_NOSE] );

				// �r
				MoveToEx( hDC, lSkeletonX[K4ABT_JOINT_HAND_LEFT], lSkeletonY[K4ABT_JOINT_HAND_LEFT], NULL );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_WRIST_LEFT], lSkeletonY[K4ABT_JOINT_WRIST_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_ELBOW_LEFT], lSkeletonY[K4ABT_JOINT_ELBOW_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_SHOULDER_LEFT], lSkeletonY[K4ABT_JOINT_SHOULDER_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_CLAVICLE_LEFT], lSkeletonY[K4ABT_JOINT_CLAVICLE_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_SPINE_CHEST], lSkeletonY[K4ABT_JOINT_SPINE_CHEST] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_CLAVICLE_RIGHT], lSkeletonY[K4ABT_JOINT_CLAVICLE_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_SHOULDER_RIGHT], lSkeletonY[K4ABT_JOINT_SHOULDER_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_ELBOW_RIGHT], lSkeletonY[K4ABT_JOINT_ELBOW_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_WRIST_RIGHT], lSkeletonY[K4ABT_JOINT_WRIST_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_HAND_RIGHT], lSkeletonY[K4ABT_JOINT_HAND_RIGHT] );

				// ����
				MoveToEx( hDC, lSkeletonX[K4ABT_JOINT_HANDTIP_LEFT], lSkeletonY[K4ABT_JOINT_HANDTIP_LEFT], NULL );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_HAND_LEFT], lSkeletonY[K4ABT_JOINT_HAND_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_THUMB_LEFT], lSkeletonY[K4ABT_JOINT_THUMB_LEFT] );

				// �E��
				MoveToEx( hDC, lSkeletonX[K4ABT_JOINT_HANDTIP_RIGHT], lSkeletonY[K4ABT_JOINT_HANDTIP_RIGHT], NULL );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_HAND_RIGHT], lSkeletonY[K4ABT_JOINT_HAND_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_THUMB_RIGHT], lSkeletonY[K4ABT_JOINT_THUMB_RIGHT] );

				// �r
				MoveToEx( hDC, lSkeletonX[K4ABT_JOINT_FOOT_LEFT], lSkeletonY[K4ABT_JOINT_FOOT_LEFT], NULL );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_ANKLE_LEFT], lSkeletonY[K4ABT_JOINT_ANKLE_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_KNEE_LEFT], lSkeletonY[K4ABT_JOINT_KNEE_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_HIP_LEFT], lSkeletonY[K4ABT_JOINT_HIP_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_PELVIS], lSkeletonY[K4ABT_JOINT_PELVIS] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_HIP_RIGHT], lSkeletonY[K4ABT_JOINT_HIP_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_KNEE_RIGHT], lSkeletonY[K4ABT_JOINT_KNEE_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_ANKLE_RIGHT], lSkeletonY[K4ABT_JOINT_ANKLE_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_FOOT_RIGHT], lSkeletonY[K4ABT_JOINT_FOOT_RIGHT] );
			}
			SelectObject( hDC, hPenPrev );
			EndPaint( hWnd, &ps );
		}
		return 0;
	case WM_CLOSE:
		DestroyWindow( hWnd );
	case WM_DESTROY:
		PostQuitMessage( 0 );
		break;
	default:
		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	}
	return 0;
}

void DestroyWs2()
{
	if ( g_pImageRBT )
	{
		delete [] g_pImageRBT;
		g_pImageRBT = nullptr;
	}
	if ( g_pWs )
	{
		g_pWs->UdpDestroySend();
		g_pWs->UdpDestroyReceive();
		delete g_pWs;
		g_pWs = nullptr;
	}
}

HRESULT CreateWs2( char* szServerName )
{
	g_pWs = new WS2;
	if ( ! g_pWs )
		return E_FAIL;

	g_pImageRBT = new RBT_STRUCT_SAVED[PACKET_DIVIDE];
	if ( ! g_pImageRBT )
	{
		delete g_pWs;
		return E_FAIL;
	}
	ZeroMemory( g_pImageRBT, sizeof(RBT_STRUCT_SAVED) * PACKET_DIVIDE );
	g_pWs->UdpCreateSend( szServerName, DESTINATION_PORT );
	g_pWs->UdpCreateReceive( SENDBACK_PORT );
	return S_OK;
}

// �A�v���P�[�V�����̏����� (�E�B���h�E��`��p�̃r�b�g�}�b�v���쐬)
HRESULT InitApp( HINSTANCE hInst, int nCmdShow, LPTSTR pCmdLine )
{
	WNDCLASSEX wc = { 0, };
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH) GetStockObject( BLACK_BRUSH );
	wc.lpszClassName = szClassName;
	wc.hIconSm = LoadIcon( NULL, IDI_APPLICATION );
	if ( ! RegisterClassEx( &wc ) )
	{
		MessageBox( NULL, TEXT("�A�v���P�[�V�����N���X�̏������Ɏ��s"), TEXT("�G���["), MB_OK );
		return E_FAIL;
	}

	// �A�v���P�[�V�����E�B���h�E���쐬����
	RECT rc = { 0 };
	rc.right = RESOLUTION_WIDTH;
	rc.bottom = RESOLUTION_HEIGHT;
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	g_hWnd = CreateWindow( szClassName, szClassName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInst, NULL );
	if ( ! g_hWnd )
	{
		MessageBox( NULL, TEXT("�E�B���h�E�̏������Ɏ��s"), TEXT("�G���["), MB_OK );
		return E_FAIL;
	}

	// ���i�`��p�̃y�����쐬����
	g_hPen = CreatePen( PS_SOLID, 3, RGB( 0, 255, 0 ) );
	if ( ! g_hPen )
	{
		MessageBox( NULL, TEXT("�y���̏������Ɏ��s"), TEXT("�G���["), MB_OK );
		return E_FAIL;
	}

	// ��ʕ\���p�̃r�b�g�}�b�v���쐬����
	ZeroMemory( &g_biBMP, sizeof(g_biBMP) );
	g_biBMP.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	g_biBMP.bmiHeader.biBitCount = 32;
	g_biBMP.bmiHeader.biPlanes = 1;
	g_biBMP.bmiHeader.biWidth = RESOLUTION_WIDTH;
	g_biBMP.bmiHeader.biHeight = -(int) RESOLUTION_HEIGHT;
	g_hBMP = CreateDIBSection( NULL, &g_biBMP, DIB_RGB_COLORS, (LPVOID*) (&g_pdwPixel), NULL, 0 );
	HDC hDC = GetDC( g_hWnd );
	g_hDCBMP = CreateCompatibleDC( hDC );
	ReleaseDC( g_hWnd, hDC );
	g_hBMPold = (HBITMAP) SelectObject( g_hDCBMP, g_hBMP );

	// �[�x�}�b�v�p�o�b�t�@���쐬
	g_pDepthMap = new UINT16[RESOLUTION_SIZE];
	memset( g_pDepthMap, 0, RESOLUTION_SIZE * sizeof(UINT16) );

	ShowWindow( g_hWnd, nCmdShow );
	UpdateWindow( g_hWnd );

	return S_OK;
}

// �A�v���P�[�V�����̌�n��
HRESULT UninitApp()
{
	// �[�x�}�b�v���������
	if ( g_pDepthMap )
	{
		delete [] g_pDepthMap;
		g_pDepthMap = NULL;
	}

	// ��ʕ\���p�̃r�b�g�}�b�v���������
	if ( g_hDCBMP || g_hBMP )
	{
		SelectObject( g_hDCBMP, g_hBMPold );
		DeleteObject( g_hBMP );
		DeleteDC( g_hDCBMP );
		g_hBMP = NULL;
		g_hDCBMP = NULL;
	}

	// �y�����������
	if ( g_hPen )
	{
		DeleteObject( (HGDIOBJ) g_hPen );
		g_hPen = NULL;
	}
	return S_OK;
}

int WINAPI _tWinMain( HINSTANCE hInst, HINSTANCE, LPTSTR pCmdLine, int nCmdShow )
{
	char szServerName[16] = DESTINATION_IP_ADDRESS;
	if ( pCmdLine && pCmdLine[0] )
		WideCharToMultiByte( CP_ACP, 0, pCmdLine, (int) _tcslen(pCmdLine), szServerName, 15, NULL, NULL );

	// �A�v���P�[�V�����̏������֐����Ă�
	if ( FAILED( InitApp( hInst, nCmdShow, pCmdLine ) ) )
		return E_FAIL;

	if ( FAILED( CreateWs2( szServerName ) ) )
	{
		UninitApp();
		return E_FAIL;
	}

	// Kinect �̏������֐����Ă�
	if ( FAILED( CreateKinectOnSite() ) )
		return 1;

	// �A�v���P�[�V�������[�v
	MSG msg;
	while( GetMessage( &msg, NULL, 0, 0 ) )
	{
		TranslateMessage( &msg );
		DispatchMessage( &msg );

		// Kinect �����֐����Ă�
		if ( KinectProcOnSite() == S_OK )
		{
			// Kinect ���ɍX�V������Ε`�惁�b�Z�[�W�𔭍s����
			InvalidateRect( g_hWnd, NULL, TRUE );
		}
	}

	// Kinect �̏I���֐����Ă�
	DestroyKinectOnSite();

	// �A�v���P�[�V�������I���֐����Ă�
	DestroyWs2();

	UninitApp();

	return 0;
}
