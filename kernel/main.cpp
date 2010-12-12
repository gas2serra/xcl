// main.cpp
//
// Copyright (C) 2006-2010 Peter Graves <gnooth@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <signal.h>

#include "lisp.hpp"
#include "Frame.hpp"
#include "primitives.hpp"
#include "call_depth_limit.hpp"
#include "xcl_home.hpp"
#include "Pathname.hpp"
#include "Readtable.hpp"

#ifdef WIN32
BOOL WINAPI control_c_handler(DWORD event)
{
  if (event == CTRL_C_EVENT)
    {
      interrupted = 1;
      return TRUE;
    }

  return FALSE;
}
#endif

void initialize_control_c_handler()
{
#ifdef WIN32
  if (SetConsoleCtrlHandler(control_c_handler, TRUE))
    {
      printf("Control-C handler installed\n");
      fflush(stdout);
    }
#endif
}

extern Frame * primordial_frame;

static SimpleString * build_date()
{
  String * s = new String(xcl_home_pathname()->namestring());
  s->append("build");
  long fd = OPEN(s->as_c_string(), O_RDONLY);
  if (fd >= 0)
    {
      char buf[256];
      long n = READ(fd, buf, sizeof(buf));
      CLOSE(fd);
      if (n > 0)
        {
          for (unsigned long i = 0; i < sizeof(buf); i++)
            {
              if (buf[i] < ' ')
                {
                  buf[i] = 0;
                  return new_simple_string(buf);
                }
            }
        }
    }
  return NULL;
}

void process_command_line(int argc, char * argv[])
{
  Value list = NIL;
  for (int i = argc; i-- > 0;)
    list = make_cons(make_simple_string(argv[i]), list);
  the_symbol(S_argv)->set_value(list);
}

void gc_warn_proc(char * msg, GC_word arg)
{
  printf(msg, (unsigned long) arg);
  fflush(stdout);
}

#ifndef WIN32
static void segv_handler(int sig, siginfo_t *si, void *unused)
{
  SYS_set_symbol_global_value(S_saved_stack, SYS_current_stack_as_list());
  SYS_set_symbol_global_value(S_saved_backtrace, current_thread()->backtrace_as_list(MOST_POSITIVE_FIXNUM));
  printf("SIGSEGV at address 0x%lx\n", (unsigned long) si->si_addr);
  siglongjmp(*primordial_frame->jmp(), 101);
}
#endif

volatile bool boot_loaded_p = false;

int __main(int argc, char * argv[])
{
//   initialize_uptime();
//   GC_init();
//   GC_set_warn_proc(gc_warn_proc);
  initialize_lisp();
  process_command_line(argc, argv);
  String * s = new String();
  // REVIEW we know it's a string (or do we?)
  s->append(check_string(CL_lisp_implementation_type()));
  s->append_char(' ');
  // REVIEW we know it's a string (or do we?)
  s->append(check_string(CL_lisp_implementation_version()));
#ifdef __x86_64__
  s->append(" (x86-64)");
#else
  s->append(" (x86)");
#endif
  SimpleString * built = build_date();
  if (built)
    {
      s->append(" built ");
      s->append(built);
//       s->append_char(')');
    }
  s->append_char('\n');
  s->append("Copyright (C) 2006-2010 Peter Graves\n");
//   s->append("Low-level initialization completed in ");
//   s->append(uptime_as_string());
//   s->append(".\n");
  STANDARD_OUTPUT->write_string(s);

  initialize_control_c_handler();

#ifndef WIN32
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = segv_handler;
  if (sigaction(SIGSEGV, &sa, NULL) == -1)
    perror("sigaction");
  if (sigaction(SIGABRT, &sa, NULL) == -1)
    perror("sigaction2");
#endif

  primordial_frame = new Frame();
  Thread * const thread = current_thread();
  Function * interactive_eval = (Function *) the_symbol(S_interactive_eval)->function();
  while (true)
    {
// #if defined(__linux__) || defined(__FreeBSD__)
//       int ret = sigsetjmp(*primordial_frame->jmp(), 1);
// #else
//       setjmp(*primordial_frame->jmp());
// #endif
#ifdef WIN32
      SETJMP(*primordial_frame->jmp());
#else
      int ret = SETJMP(*primordial_frame->jmp());
#endif

      thread->set_stack(0);
      thread->set_call_depth(0);
      thread->set_last_control_frame(NULL);
      thread->set_last_tag(NULL);

      call_depth_limit = DEFAULT_CALL_DEPTH_LIMIT;

#ifndef WIN32
      if (ret == 101)
        {
          if (CL_fboundp(S_invoke_debugger_internal))
            current_thread()->execute(the_symbol(S_invoke_debugger_internal)->function(), make_value(new Error("SIGSEGV")));
        }
#endif
      if (!boot_loaded_p)
        {
          boot_loaded_p = true; // only try once!
          if (SYS_load_system_file(make_simple_string("lisp/boot.lisp")) != NIL)
            {
              s = new String("Startup completed in ");
              s->append(uptime_as_string());
              s->append(".\n");
              STANDARD_OUTPUT->write_string(s);
            }
        }

      Value repl = thread->symbol_value(S_top_level_read_eval_print_loop);
      if (functionp(repl))
        thread->execute(the_function(repl));
      else
        {
          while (true)
            {
              Stream * out = check_stream(thread->symbol_value(S_standard_output));
              out->write_string("* ");
              out->finish_output();
              Stream * in = check_stream(thread->symbol_value(S_standard_input));
              Readtable * rt = check_readtable(thread->symbol_value(S_current_readtable));
              Value obj = in->read(true, NIL, false, thread, rt);
              Value result = thread->execute(interactive_eval, obj);
              assert(thread->stack() == 0);
              long values_length = thread->values_length();
              if (values_length < 0)
                {
                  // single value
                  out->terpri();
                  out->prin1(result);
                }
              else
                {
                  // multiple values
                  Value * values = thread->values();
                  for (long i = 0; i < values_length; i++)
                    {
                      out->terpri();
                      out->prin1(values[i]);
                    }
                }
              out->terpri();
            }
        }
    }
  return 0;
}

#ifdef WIN32
struct arginfo
{
  int argc;
  char * * argv;
};

DWORD WINAPI main_thread_proc(LPVOID p)
{
  arginfo * args = (arginfo *) p;
  return __main(args->argc, args->argv);
}

int _main(int argc, char * argv[])
{
  arginfo args;
  args.argc = argc;
  args.argv = argv;

  DWORD thread_id;

#ifndef STACK_SIZE_PARAM_IS_A_RESERVATION
#define STACK_SIZE_PARAM_IS_A_RESERVATION 0x00010000
#endif

  HANDLE h = GC_CreateThread(NULL,                              // default security descriptor
                             4194304,                           // stack size
                             main_thread_proc,
                             &args,
                             STACK_SIZE_PARAM_IS_A_RESERVATION, // creation flags
                             &thread_id);
  WaitForSingleObject(h, INFINITE);
  return 0;
}
#endif

int main(int argc, char * argv[])
{
  initialize_uptime();

  stack_top = current_sp();

  GC_init();
  GC_set_warn_proc(gc_warn_proc);

#ifdef WIN32
  return _main(argc, argv);
#else
  return __main(argc, argv);
#endif
}
