# myshell - Phase 1

이 프로젝트는 간단한 유닉스 쉘을 구현한 것입니다. **Phase 1**에서는 기본적인 명령어 실행 및 백그라운드 프로세스 처리, 그리고 `SIGCHLD` 시그널 처리를 통한 좀비 프로세스 방지가 핵심입니다.

## 📌 핵심 기능 (Phase 1)
- `fork()`를 이용한 자식 프로세스 생성
- `execve()`로 명령어 실행 (`/bin/` 경로 자동 prepend)
- `SIGCHLD` 핸들러를 통한 좀비 프로세스 회수
- `cd`, `quit` 명령어 내장 처리
- 백그라운드 명령 (`&`) 처리

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
CSE4100-SP-P2> ls
CSE4100-SP-P2> echo hello
hello
CSE4100-SP-P2> cd ..
CSE4100-SP-P2> ./a.out &
[3421] ./a.out &
CSE4100-SP-P2> quit
```

- `cd`, `quit`는 쉘 내부에서 처리되는 **builtin command**입니다.
- `&`는 **백그라운드 실행**을 의미하며, PID와 명령어가 출력됩니다.
- `SIGCHLD`는 `sigsuspend` 및 `waitpid`를 통해 안전하게 처리되어, **좀비 프로세스가 생기지 않습니다.**

---

## 🧹 클린업

컴파일된 파일들을 제거하려면:

```bash
make clean
```
