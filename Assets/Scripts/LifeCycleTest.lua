
ElapsedTime = 0
HasPrintedTick = false

function BeginPlay()
    print("Lua BeginPlay")
end

function Tick(DeltaTime)
    ElapsedTime = ElapsedTime + DeltaTime

    -- Tick 호출은 한 번만 출력한다.
    if not HasPrintedTick then
        print("Lua Tick", DeltaTime)
        HasPrintedTick = true
    end
end

function EndPlay()
    print("Lua EndPlay", ElapsedTime)
end