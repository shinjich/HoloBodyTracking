#ifndef STRICT
#define STRICT
#endif
#include "udp.h"
#include "BodyStruct.h"
#include "BitSaving.h"
#include <tchar.h>

// Win32 アプリ用
static const TCHAR szClassName[] = TEXT("RemoteBodyTracking4PC");
HWND g_hWnd = NULL;							// アプリケーションのウィンドウ
HBITMAP g_hBMP = NULL, g_hBMPold = NULL;	// 表示するビットマップのハンドル
HDC g_hDCBMP = NULL;						// 表示するビットマップのコンテキスト
BITMAPINFO g_biBMP = { 0, };				// ビットマップの情報 (解像度やフォーマット)
LPDWORD g_pdwPixel = NULL;					// ビットマップの中身の先頭 (ピクセル情報)
UINT16* g_pDepthMap = NULL;					// 深度マップバッファのポインタ
UINT16* g_pIrMap = NULL;					// IR マップバッファのポインタ
BITSAVING g_bsave;

// ボディ描画用
HPEN g_hPen = NULL;
k4abt_skeleton_t g_Skeleton[MAX_BODIES];
k4a_float2_t g_fSkeleton2D[MAX_BODIES][K4ABT_JOINT_COUNT] = { 0.0f, };
uint32_t g_uBodyID[MAX_BODIES] = { K4ABT_INVALID_BODY_ID, };
uint32_t g_uBodies = 0;

// Azure Kinect 用
k4abt_tracker_t hTracker = nullptr;
k4a_calibration_t remoteCalibration;
k4a_capture_t remoteCapture = nullptr;
k4a_image_t remoteImageDepth = nullptr;
k4a_image_t remoteImageIR = nullptr;

WS2* g_pWs = nullptr;

// Kinect を終了する
void DestroyKinect()
{
	// 骨格追跡を終了
	if ( hTracker )
	{
		k4abt_tracker_shutdown( hTracker );
		k4abt_tracker_destroy( hTracker );
		hTracker = nullptr;
	}
	k4a_capture_release( remoteCapture );
}

// Kinect を初期化する
HRESULT CreateKinect()
{
	BYTE s_intr[64] = {
		0x04, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x15, 0x6e, 0x7c, 0x43, 0xcf, 0x31, 0x81, 0x43,
		0x0e, 0x83, 0x7c, 0x43, 0x37, 0x92, 0x7c, 0x43, 0x3c, 0x39, 0x84, 0x3f, 0x04, 0x32, 0x16, 0x3f,
		0xf8, 0x42, 0x07, 0x3d, 0xec, 0x43, 0xaf, 0x3f, 0xc4, 0xbd, 0x60, 0x3f, 0x1a, 0x89, 0x2c, 0x3e,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xae, 0xe2, 0xd3, 0xb8, 0x01, 0x5c, 0x91, 0xb8
	};
	ZeroMemory( &remoteCalibration, sizeof(remoteCalibration) );
	remoteCalibration.depth_mode = K4A_DEPTH_MODE;
#if RBT_LONGTHROW_DEPTH
	s_intr[10] = 0x1c;
	s_intr[12] = 0x9e;
	s_intr[13] = 0x63;
	s_intr[14] = 0x28;
#endif
	CopyMemory( &remoteCalibration.depth_camera_calibration.intrinsics, s_intr, sizeof(s_intr) );

	remoteCalibration.depth_camera_calibration.resolution_width = RESOLUTION_WIDTH;
	remoteCalibration.depth_camera_calibration.resolution_height = RESOLUTION_HEIGHT;
	remoteCalibration.depth_camera_calibration.metric_radius = 1.74f;
	remoteCalibration.depth_camera_calibration.extrinsics.rotation[0] = remoteCalibration.depth_camera_calibration.extrinsics.rotation[4] = remoteCalibration.depth_camera_calibration.extrinsics.rotation[8] = 1.0f;

	k4a_capture_create( &remoteCapture );
	k4a_image_create( K4A_IMAGE_FORMAT_DEPTH16, RESOLUTION_WIDTH, RESOLUTION_HEIGHT, RESOLUTION_WIDTH * sizeof(uint16_t), &remoteImageDepth );
	k4a_capture_set_depth_image( remoteCapture, remoteImageDepth );
	k4a_image_create( K4A_IMAGE_FORMAT_IR16, RESOLUTION_WIDTH, RESOLUTION_HEIGHT, RESOLUTION_WIDTH * sizeof(uint16_t), &remoteImageIR );
	k4a_capture_set_ir_image( remoteCapture, remoteImageIR );
	_k4abt_tracker_configuration_t tracker_config = ::K4ABT_TRACKER_CONFIG_DEFAULT;
	tracker_config.processing_mode = K4ABT_TRACKER_PROCESSING_MODE_GPU_CUDA;
	if ( k4abt_tracker_create( &remoteCalibration, tracker_config, &hTracker ) == K4A_RESULT_FAILED )
		return E_FAIL;

	return S_OK;
}

HRESULT CopyToDepthIrBuffer( uint8_t* pDstDepth, uint8_t* pDstIR )
{
	HRESULT hr = E_FAIL;
	for( ; ; )
	{
		RBT_STRUCT_SAVED rbt = { 0, };
		int iReceived = g_pWs->UdpReceiveData( (char*) &rbt, sizeof(RBT_STRUCT_SAVED) );
		if ( iReceived > 0 )
		{
			const DWORD dwOffset = rbt.dwHeader * (RESOLUTION_SIZE / PACKET_DIVIDE);
			g_bsave.Restore( &g_pDepthMap[dwOffset], rbt.cDepth,  RESOLUTION_SIZE / PACKET_DIVIDE, SCOPE_DISTANCE_BITS_D );
			for( int i = 0; i < RESOLUTION_SIZE / PACKET_DIVIDE; i++ )
				((UINT16*) pDstDepth)[dwOffset + i] = g_pDepthMap[dwOffset + i] + MIN_OFFSET_DISTANCE;
			g_bsave.Restore( &g_pIrMap[dwOffset], rbt.cIr,  RESOLUTION_SIZE / PACKET_DIVIDE, SCOPE_DISTANCE_BITS_I );
			CopyMemory( &((UINT16*) pDstIR)[dwOffset], &g_pIrMap[dwOffset], RESOLUTION_SIZE * sizeof(UINT16) / PACKET_DIVIDE );
			hr = S_OK;
		}
		else
			break;
	}
	return hr;
}

// Kinect のメインループ処理
HRESULT KinectProc()
{
	k4a_wait_result_t kr = K4A_WAIT_RESULT_FAILED;

	// 骨格追跡
	uint8_t* pDstDepth = k4a_image_get_buffer( remoteImageDepth );
	uint8_t* pDstIR = k4a_image_get_buffer( remoteImageIR );

	HRESULT hr = CopyToDepthIrBuffer( pDstDepth, pDstIR );
	if ( hr == S_OK )
	{
		hr = E_FAIL;
		kr = k4abt_tracker_enqueue_capture( hTracker, remoteCapture, K4A_WAIT_INFINITE );

		if ( kr == K4A_WAIT_RESULT_SUCCEEDED )
		{
			// 骨格追跡の結果を取得
			k4abt_frame_t hBodyFrame = nullptr;
			kr = k4abt_tracker_pop_result( hTracker, &hBodyFrame, K4A_WAIT_INFINITE );
			if ( kr == K4A_WAIT_RESULT_SUCCEEDED )
			{
				// 認識された人数を取得
				uint32_t uBodies = k4abt_frame_get_num_bodies( hBodyFrame );
				if ( uBodies > MAX_BODIES )
					uBodies = MAX_BODIES;
				for( uint32_t uBody = 0; uBody < uBodies; uBody++ )
				{
					k4abt_skeleton_t skeleton;
					// 骨格情報を取得
					if ( k4abt_frame_get_body_skeleton( hBodyFrame, uBody, &skeleton ) == K4A_RESULT_SUCCEEDED )
					{
						// 描画用のコピー
						CopyMemory( &g_Skeleton[uBody], &skeleton, sizeof(k4abt_skeleton_t) );
						uint32_t uBodyID = k4abt_frame_get_body_id( hBodyFrame, uBody );
						g_uBodyID[uBody] = uBodyID;
						for( int iJoint = K4ABT_JOINT_PELVIS; iJoint < K4ABT_JOINT_COUNT; iJoint++ )
						{
							int iValid = 0;
							// 骨格座標から 2D 座標に変換
							k4a_float2_t skeleton2d;
							k4a_calibration_3d_to_2d( &remoteCalibration, &skeleton.joints[iJoint].position, K4A_CALIBRATION_TYPE_DEPTH, K4A_CALIBRATION_TYPE_DEPTH, &skeleton2d, &iValid );
							if ( iValid == 0 )
							{
								// 無効な値を 0 にする
								g_fSkeleton2D[uBody][iJoint].xy.x = g_fSkeleton2D[uBody][iJoint].xy.y = 0.0f;
							}
							else
							{
								g_fSkeleton2D[uBody][iJoint].xy.x = skeleton2d.xy.x;
								g_fSkeleton2D[uBody][iJoint].xy.y = skeleton2d.xy.y;
							}
						}
					}
				}
				k4abt_frame_release( hBodyFrame );
				g_uBodies = uBodies;
				if ( uBodies )
				{
					SBT_STRUCT sbt;
					sbt.dwHeader = uBodies;
					CopyMemory( &sbt.skeleton[0], &g_Skeleton[0], uBodies * sizeof(k4abt_skeleton_t) );
					CopyMemory( &sbt.skeleton2D[0][0], &g_fSkeleton2D[0][0], uBodies * K4ABT_JOINT_COUNT * sizeof(k4a_float2_t) );
					CopyMemory( &sbt.bodyID[0], &g_uBodyID[0], uBodies * sizeof(uint32_t) );
					g_pWs->UdpSendData( (char*) &sbt, sizeof(SBT_STRUCT) );
				}
				return S_OK;
			}
		}
	}
	return hr;
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch( uMsg )
	{
	case WM_PAINT:
		{
			// 骨格の描画
			TCHAR szText[128];
			PAINTSTRUCT ps;
			HDC hDC = BeginPaint( hWnd, &ps );

			// 画面サイズを取得する
			RECT rect;
			GetClientRect( hWnd, &rect );

			// 16-bit 深度マップから 32-bit カラービットマップに変換する
			for( uint32_t u = 0; u < RESOLUTION_SIZE; u++ )
			{
				const WORD w = g_pDepthMap[u];
				g_pdwPixel[u] = 0xFF000000 | (w << 16) | (w << 8) | w;
			}

			// カラー化した深度の表示
			StretchBlt( hDC, 0, 0, rect.right, rect.bottom, g_hDCBMP, 0, 0, RESOLUTION_WIDTH, RESOLUTION_HEIGHT, SRCCOPY );

			// 深度イメージに対するスクリーンサイズの比を計算
			const FLOAT fRelativeWidth = (FLOAT) rect.right / (FLOAT) RESOLUTION_WIDTH;
			const FLOAT fRelativeHeight = (FLOAT) rect.bottom / (FLOAT) RESOLUTION_HEIGHT;

			// 描画用のペンを選択
			HPEN hPenPrev = (HPEN) SelectObject( hDC, g_hPen );
			SetBkMode( hDC, TRANSPARENT );

			for( uint32_t uBody = 0; uBody < g_uBodies; uBody++ )
			{
				// スクリーンサイズに骨格をスケーリング
				LONG lSkeletonX[K4ABT_JOINT_COUNT];
				LONG lSkeletonY[K4ABT_JOINT_COUNT];
				for( int iJoint = K4ABT_JOINT_PELVIS; iJoint < K4ABT_JOINT_COUNT; iJoint++ )
				{
					lSkeletonX[iJoint] = (LONG) (g_fSkeleton2D[uBody][iJoint].xy.x * fRelativeWidth);
					lSkeletonY[iJoint] = (LONG) (g_fSkeleton2D[uBody][iJoint].xy.y * fRelativeHeight);

					// 座標を表示
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
				
				// 顔
				MoveToEx( hDC, lSkeletonX[K4ABT_JOINT_EAR_LEFT], lSkeletonY[K4ABT_JOINT_EAR_LEFT], NULL );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_EYE_LEFT], lSkeletonY[K4ABT_JOINT_EYE_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_NOSE], lSkeletonY[K4ABT_JOINT_NOSE] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_EYE_RIGHT], lSkeletonY[K4ABT_JOINT_EYE_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_EAR_RIGHT], lSkeletonY[K4ABT_JOINT_EAR_RIGHT] );

				// 体
				MoveToEx( hDC, lSkeletonX[K4ABT_JOINT_PELVIS], lSkeletonY[K4ABT_JOINT_PELVIS], NULL );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_SPINE_NAVEL], lSkeletonY[K4ABT_JOINT_SPINE_NAVEL] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_SPINE_CHEST], lSkeletonY[K4ABT_JOINT_SPINE_CHEST] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_NECK], lSkeletonY[K4ABT_JOINT_NECK] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_HEAD], lSkeletonY[K4ABT_JOINT_HEAD] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_NOSE], lSkeletonY[K4ABT_JOINT_NOSE] );

				// 腕
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

				// 左手
				MoveToEx( hDC, lSkeletonX[K4ABT_JOINT_HANDTIP_LEFT], lSkeletonY[K4ABT_JOINT_HANDTIP_LEFT], NULL );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_HAND_LEFT], lSkeletonY[K4ABT_JOINT_HAND_LEFT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_THUMB_LEFT], lSkeletonY[K4ABT_JOINT_THUMB_LEFT] );

				// 右手
				MoveToEx( hDC, lSkeletonX[K4ABT_JOINT_HANDTIP_RIGHT], lSkeletonY[K4ABT_JOINT_HANDTIP_RIGHT], NULL );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_HAND_RIGHT], lSkeletonY[K4ABT_JOINT_HAND_RIGHT] );
				LineTo( hDC, lSkeletonX[K4ABT_JOINT_THUMB_RIGHT], lSkeletonY[K4ABT_JOINT_THUMB_RIGHT] );

				// 脚
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
	if ( g_pWs )
	{
		g_pWs->UdpDestroyReceive();
		g_pWs->UdpDestroySend();
		delete g_pWs;
		g_pWs = nullptr;
	}
}

HRESULT CreateWs2( char* szClientName )
{
	g_pWs = new WS2;
	if ( ! g_pWs )
		return E_FAIL;

	g_pWs->UdpCreateReceive( DESTINATION_PORT );
	g_pWs->UdpCreateSend( szClientName, SENDBACK_PORT );
	return S_OK;
}

// アプリケーションの初期化 (ウィンドウや描画用のビットマップを作成)
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
		MessageBox( NULL, TEXT("アプリケーションクラスの初期化に失敗"), TEXT("エラー"), MB_OK );
		return E_FAIL;
	}

	// アプリケーションウィンドウを作成する
	RECT rc = { 0 };
	rc.right = RESOLUTION_WIDTH;
	rc.bottom = RESOLUTION_HEIGHT;
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	g_hWnd = CreateWindow( szClassName, szClassName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInst, NULL );
	if ( ! g_hWnd )
	{
		MessageBox( NULL, TEXT("ウィンドウの初期化に失敗"), TEXT("エラー"), MB_OK );
		return E_FAIL;
	}

	// 骨格描画用のペンを作成する
	g_hPen = CreatePen( PS_SOLID, 3, RGB( 0, 255, 0 ) );
	if ( ! g_hPen )
	{
		MessageBox( NULL, TEXT("ペンの初期化に失敗"), TEXT("エラー"), MB_OK );
		return E_FAIL;
	}

	// 画面表示用のビットマップを作成する
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

	// 深度マップ用バッファを作成
	g_pDepthMap = new UINT16[RESOLUTION_SIZE];
	memset( g_pDepthMap, 0, RESOLUTION_SIZE * sizeof(UINT16) );

	g_pIrMap = new UINT16[RESOLUTION_SIZE];
	memset( g_pIrMap, 0, RESOLUTION_SIZE * sizeof(UINT16) );

	ShowWindow( g_hWnd, nCmdShow );
	UpdateWindow( g_hWnd );

	return S_OK;
}

// アプリケーションの後始末
HRESULT UninitApp()
{
	// 深度マップを解放する
	if ( g_pIrMap )
	{
		delete [] g_pIrMap;
		g_pIrMap = NULL;
	}
	if ( g_pDepthMap )
	{
		delete [] g_pDepthMap;
		g_pDepthMap = NULL;
	}

	// 画面表示用のビットマップを解放する
	if ( g_hDCBMP || g_hBMP )
	{
		SelectObject( g_hDCBMP, g_hBMPold );
		DeleteObject( g_hBMP );
		DeleteDC( g_hDCBMP );
		g_hBMP = NULL;
		g_hDCBMP = NULL;
	}

	// ペンを解放する
	if ( g_hPen )
	{
		DeleteObject( (HGDIOBJ) g_hPen );
		g_hPen = NULL;
	}
	return S_OK;
}

int WINAPI _tWinMain( HINSTANCE hInst, HINSTANCE, LPTSTR pCmdLine, int nCmdShow )
{
	char szClientName[16] = SENDBACK_IP_ADDRESS;
	if ( pCmdLine && pCmdLine[0] )
		WideCharToMultiByte( CP_ACP, 0, pCmdLine, (int) _tcslen(pCmdLine), szClientName, 15, NULL, NULL );

	// アプリケーションの初期化関数を呼ぶ
	if ( FAILED( InitApp( hInst, nCmdShow, pCmdLine ) ) )
		return E_FAIL;

	if ( FAILED( CreateWs2( szClientName ) ) )
		return E_FAIL;

	// Kinect の初期化関数を呼ぶ
	if ( FAILED( CreateKinect() ) )
		return 1;

	// アプリケーションループ
	MSG msg = { 0, };
	while( msg.message != WM_QUIT )
	{
		if ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}

		// Kinect 処理関数を呼ぶ
		if ( KinectProc() == S_OK )
		{
			// Kinect 情報に更新があれば描画メッセージを発行する
			InvalidateRect( g_hWnd, NULL, TRUE );
		}
	}

	// Kinect の終了関数を呼ぶ
	DestroyKinect();

	// アプリケーションを終了関数を呼ぶ
	DestroyWs2();

	UninitApp();

	return 0;
}
