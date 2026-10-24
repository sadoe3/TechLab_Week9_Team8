-- 파일을 실행할 때마다 생성되며 ScriptComponent 사이에 공유되지 않는다.
local ElapsedTime = 0

-- 엔진이 이름으로 찾으므로 콜백 함수에는 local을 붙이지 않는다.

-- 스크립트 시작 시 한 번 호출된다.
function BeginPlay()
    print("Lua BeginPlay")
end

-- 매 프레임 호출된다. DeltaTime의 단위는 초다.
function Tick(DeltaTime)
    ElapsedTime = ElapsedTime + DeltaTime

    -- 이 액터의 매 프레임 동작을 작성한다.
end

-- 정상적으로 시작한 스크립트가 종료될 때 호출된다.
function EndPlay()
    print("Lua EndPlay", ElapsedTime)

    -- 스크립트에서 시작한 작업을 정리한다.
end

-- C++에서 전달한 겹침 시작 이벤트를 처리한다.
function OnOverlap(OtherActor)
    print("Lua OnOverlap", OtherActor ~= nil)
end
