Racing 에셋 적용본

열기: Assets/Scenes/MainScene_AssetShowcase.scene
원본: Assets/Scenes/MainScene.scene (원본 파일 보존)

폴더
- CarKit / RacingKit / PlatformerKit / NatureKit / ToyCarKit: Kenney 대표 모델 24종
- Prepared: 플레이어 루트·충돌 크기 보존용 차량 파생 OBJ
- Track: 기존 트랙·벽 크기에 맞춘 타일 UV 메시와 표면 MTL
- Assets/Textures/Racing: 표면 8종 Albedo/NormalDX 및 Effects PNG

씬 적용
- 플레이어 차량, 코인 20개, 도로, 벽, 잔디, 늪 표면 교체
- 관중석, 피트, 주차 차량, 결승 게이트, 나무, 바위, 소품 배치
- 추가 지면·장식의 충돌 비활성
- 기존 Actor 이름, 스크립트, 차량 Capsule 및 트랙 충돌 설정 보존
- 복사본 ASwamp_0.BoxComponent의 어긋난 상대 위치만 원점으로 정렬
- 복사본 플레이어 추적 카메라 위치 조정 및 조명 추가
- C++·Lua 원본, 게임 시작 씬 경로 변경 없음

출처와 라이선스
- Kenney: https://kenney.nl/assets (각 팩 License.txt 및 provenance.assetinfo)
- Poly Haven: https://polyhaven.com (각 텍스처 provenance.assetinfo)
- ambientCG Grass004: https://ambientcg.com/view?id=Grass004
- 에셋 라이선스 CC0 1.0, 각 원본 라이선스 동봉
- 엔진의 OBJ 선행 로드를 고려한 대표 모델만 실행 폴더에 포함

전체 팩 원본 ZIP 및 검증 기록
Build/Validation/RacingAssetScene20261026/

렌더 범위
- 기존 Game 임시 렌더 경로 기준. 그림자·Fog 후처리 통합은 별도 작업
- 늪의 연무 외형은 기존 ParticleSubUV 컴포넌트로 표시
