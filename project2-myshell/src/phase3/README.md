# myshell - Phase 3 (Advanced Job Control)

이 프로젝트의 **Phase 3**에서는 프로세스 그룹과 시그널 제어를 활용한 **고급 잡 관리 기능**을 구현합니다. 이를 통해 사용자 정의 쉘이 실제 유닉스 쉘처럼 복잡한 시나리오에서도 동작할 수 있게 됩니다.

## 📌 핵심 기능 (Phase 3)
- 프로세스 그룹 기반 job 관리 (`setpgid`, `kill`, `waitpid`)
- job 상태 추적: `Running`, `Foreground`, `Stopped`
- 사용자 입력에 따른 job 제어 (`fg`, `bg`, `kill`)
- `SIGINT`, `SIGTSTP`, `SIGCHLD` 시그널 핸들링 강화
- 파이프라인을 포함한 명령어들도 하나의 job으로 관리
- 각 job은 별도의 프로세스 그룹으로 실행

---

## 🔧 컴파일 방법

Makefile이 제공되므로 다음 명령어로 빌드합니다:

```bash
make
```

생성된 실행 파일 이름은 `myshell`입니다.

> ⚠️ 의존 파일: `myshell.c`, `csapp.c`, `csapp.h`

---

## ▶️ 실행 방법

```bash
./myshell
```

쉘 프롬프트는 아래와 같이 나타납니다:

```
CSE4100-SP-P2>
```

---

## 💻 사용 예시

```bash
CSE4100-SP-P2> sleep 10 &
[1] 12345

CSE4100-SP-P2> jobs
[1] (12345) Running sleep 10 &

CSE4100-SP-P2> fg %1
<sleep 10가 전경으로 실행됨>

CSE4100-SP-P2> bg %1
[1] (12345) sleep 10 &

CSE4100-SP-P2> kill %1
```

- `[jid] (pid)` 형식으로 백그라운드 잡이 표시됩니다.
- `fg`, `bg`, `kill` 명령은 잡의 상태를 제어합니다.
- `Ctrl+C`(`SIGINT`) 및 `Ctrl+Z`(`SIGTSTP`) 입력 시 적절한 job에 시그널을 전송합니다.
- 멀티프로세스 파이프라인 명령어도 하나의 job으로 처리됩니다.

---

## 🧹 클린업

컴파일된 파일들을 제거하려면:

```bash
make clean
```
