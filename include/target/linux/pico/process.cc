#ifndef PICOLIB_PROCESS_IMPL_H_
#define PICOLIB_PROCESS_IMPL_H_

namespace Pico {

    constexpr int COMM_MAX = 16;

    METHOD
    Process Process::current()
    {
        return Process( Syscall::getpid() );
    }

    METHOD
    Process Process::parent()
    {
        return Process( Syscall::getppid() );
    }

    METHOD
    Thread Thread::current()
    {
        return Thread( Syscall::gettid() );
    }

    METHOD
    void Thread::set_name(const char *comm)
    {
        Syscall::prctl(PR_SET_NAME,  (unsigned long) comm, 0, 0, 0);
    } 

    METHOD
    Process Process::find_by_name(char *proc_name)
    {
        pid_t result = 0;

        Filesystem::Directory::each("/proc", [proc_name,&result](const char *filename) -> int {
            char comm_path[PATH_MAX];
            char comm[COMM_MAX + 1];

            pid_t pid = atoi(filename);
            if ( pid == 0 )
                return 0;

            sprintf(comm_path, "/proc/%s/comm", filename);
            int fd = Syscall::open(comm_path, O_RDONLY);
            ssize_t n = Syscall::read(fd, comm, sizeof(comm));
            comm[n - 1] = '\0';
            Syscall::close(fd);

            if ( String(proc_name) == comm )
            {
                result = pid;
                return 1;
            }

            return 0;
        });

        return Process(result);
    }

    METHOD
    Process Process::find_by_path(char *exe_path)
    {
        pid_t result = 0;

        Filesystem::Directory::each("/proc", [exe_path,&result](const char *filename) {
            char link_path[PATH_MAX];
            char exe[PATH_MAX + 1];
            pid_t pid = atoi(filename);

            if ( pid == 0 )
                return 0;

            sprintf(link_path, "/proc/%s/exe", filename);
            ssize_t n = Syscall::readlink(link_path, exe, sizeof(exe));
            if ( n < 0 )
                return 0;

            exe[n] = '\0';
            if ( String(exe_path) == exe )
            {
                result = pid;
                return 1;
            }

            return 0;
        });

        return Process(result);
    }

    NO_RETURN METHOD
    void Thread::exit(int status)
    {
        Syscall::exit(status);
    }

    NO_RETURN METHOD
    void Process::exit(int status)
    {
        Syscall::exit_group(status);
    }
    
    METHOD
    Thread Thread::create(thread_routine thread_entry, void *arg)
    {
        void *child_stack;
        size_t stack_size = STACK_SIZE;
        pid_t tid;

        child_stack = Memory::allocate(stack_size, Memory::READ | Memory::WRITE | Memory::STACK);

        tid = Syscall::clone(
            CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM,
            static_cast<char *>(child_stack) + stack_size,
            NULL, NULL, NULL
        );

        if ( !tid )
            Thread::exit(thread_entry(arg));
        else
            return Thread(tid);
    }

    NO_RETURN METHOD
    void Process::execute(const char *filename, char *const argv[], char *const envp[])
    {
        Syscall::execve(filename, argv, envp);
    }

    METHOD
    Process Process::spawn(const char *filename, char *const argv[], char *const envp[])
    {
        pid_t pid = Syscall::fork();
        if ( pid == 0 )
            execute(filename, argv, envp);
        else
            return Process(pid);
    }

    template <enum channel_mode M>
    METHOD
    Process Process::spawn(Channel<M> channel, const char *filename, char *const argv[], char *const envp[])
    {
        pid_t pid = Syscall::fork();
        if ( pid == 0 )
        {
            execute(channel, filename, argv, envp);
        }
        else
            return Process(pid);
    }

    METHOD
    sighandler_t Process::set_signal_handler(int signal, sighandler_t handler)
    {
        struct sigaction act, old_act;

        act.sa_handler = handler;
        Memory::zero(&act.sa_mask, sizeof(sigset_t));
        act.sa_flags = SA_RESETHAND;

        Syscall::sigaction(signal, &act, &old_act);

        return (sighandler_t) old_act.sa_restorer;
    }

    METHOD
    int Thread::wait(int *status)
    {
        return Syscall::wait4(tid, status, 0, nullptr);
    }

    METHOD
    int Process::wait(int *status)
    {
        return Syscall::wait4(pid, status, 0, nullptr);
    }

    METHOD
    int Thread::signal(int signal)
    {
        return Syscall::tkill(tid, signal);
    }

    METHOD
    int Process::signal(int signal)
    {
        return Syscall::kill(pid, signal);
    }

    METHOD
    int Thread::kill()
    {
        return signal(SIGKILL);
    }
    
    METHOD
    int Process::kill()
    {
        return signal(SIGKILL);
    }

    namespace Filesystem {

        #if SYSCALL_EXISTS(execveat)
        NO_RETURN METHOD
        void File::execute(char *const argv[], char *const envp[])
        {
            Syscall::execveat(this->file_desc(), "", argv, envp, AT_EMPTY_PATH);
        }
        #endif
    }
}

#endif
