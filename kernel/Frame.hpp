// Frame.hpp
//
// Copyright (C) 2006-2011 Peter Graves <gnooth@gmail.com>
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

#ifndef __FRAME_HPP
#define __FRAME_HPP

class UnwindProtect;

enum FrameType
{
  PRIMORDIAL = 0,
  TAGBODY = 1,
  BLOCK = 2,
  CATCH = 3,
  UNWIND_PROTECT = 4
};

class Frame : public gc
{
private:
  FrameType _type;
  JMP_BUF _jmp;
  void * _last_special_binding;
  Frame * _last_control_frame;
  Tag * _last_tag;
  UnwindProtect * _unwind_protect;
  StackFrame * _stack;
  unsigned int _call_depth;
  Frame * _next;

  void init()
  {
    init(current_thread());
  }

protected:
  Value _block_name;
  Value _catch_tag;

public:
  void init(Thread * thread)
  {
    _last_special_binding = thread->last_special_binding();
    _last_control_frame = thread->last_control_frame();
    _last_tag = thread->last_tag();
    _unwind_protect = thread->unwind_protect();
    _stack = thread->stack();
    _call_depth = thread->call_depth();
  }

  Frame()
    : _type(PRIMORDIAL), _next(NULL)
  {
    init();
  }

  Frame(FrameType type)
    : _type(type)
  {
  }

  Frame(FrameType type, Frame * next)
    : _type(type), _next(next)
  {
    init();
  }

  Frame(FrameType type, Frame * next, Thread * thread)
    : _type(type), _next(next)
  {
    init(thread);
  }

  void clear()
  {
    memset(this, 0, sizeof(*this));
  }

  FrameType type()
  {
    return _type;
  }

  void set_type(FrameType type)
  {
    _type = type;
  }

  JMP_BUF * jmp()
  {
    return &(this->_jmp);
  }

  void * last_special_binding() const
  {
    return _last_special_binding;
  }

  Frame * last_control_frame() const
  {
    return _last_control_frame;
  }

  Tag * last_tag() const
  {
    return _last_tag;
  }

  UnwindProtect * unwind_protect() const
  {
    return _unwind_protect;
  }

  StackFrame * stack() const
  {
    return _stack;
  }

  unsigned int call_depth() const
  {
    return _call_depth;
  }

  Frame * next() const
  {
    return _next;
  }

  void set_next(Frame * frame)
  {
    if (frame == this)
      {
        printf("Frame::set_next() frame == this\n");
        extern Value SYS_int3();
        SYS_int3();
      }
    _next = frame;
  }
};

class Tag : public gc
{
private:
  Value _name; // symbol or integer
  Tagbody * _tagbody;
  Value _continuation;
  int _index;
  Tag * _next;

public:
  Tag(Value name, Tagbody * tagbody, Value continuation, int index, Tag * next)
    : _name(name), _tagbody(tagbody), _continuation(continuation), _index(index), _next(next)
  {
  }

  Tag(Value name, Tagbody * tagbody, Value continuation, Tag * next)
    : _name(name), _tagbody(tagbody), _continuation(continuation), _index(0), _next(next)
  {
  }

  Tag(Value name, Tagbody * tagbody, int index, Tag * next)
    : _name(name), _tagbody(tagbody), _continuation(0), _index(index), _next(next)
  {
  }

  Value name() const
  {
    return _name;
  }

  Tagbody * tagbody() const
  {
    return _tagbody;
  }

  Value continuation() const
  {
    return _continuation;
  }

  int index () const
  {
    return _index;
  }

  Tag * next() const
  {
    return _next;
  }
};

class Tagbody : public Frame
{
// private:
//   Tag * _tags;

public:
  Tagbody(Thread * thread)
    : Frame(TAGBODY, thread->last_control_frame(), thread)
  {
    thread->set_last_control_frame(this);
  }

//   Tag * tags() const
//   {
//     return _tags;
//   }

//   void set_tags(Tag * tag)
//   {
//     _tags = tag;
//   }
};

class Block : public Frame
{
public:
  Block()
    : Frame(BLOCK)
  {
  }

  Value name() const
  {
    return _block_name;
  }

  void set_name(Value arg)
  {
    _block_name = arg;
  }
};

#define FRAME_POOL_SIZE 128

class FramePool : public gc
{
private:
  Frame * _pool[FRAME_POOL_SIZE];
  unsigned int index;

public:
  FramePool()
    : index(0)
  {
    memset(_pool, 0, FRAME_POOL_SIZE * sizeof(Frame *));
  }

  Frame * get_frame()
  {
    return index > 0 ? _pool[--index] : NULL;
  }

  void release_frame(Frame * frame)
  {
//     for (unsigned int i = 0; i < index; i++)
//       {
//         if (_pool[i] == frame)
//           {
//             printf("frame already in pool\n");
//             extern Value SYS_int3();
//             SYS_int3();
//           }
//       }
    if (index < FRAME_POOL_SIZE - 1)
      {
        frame->clear();
        _pool[index++] = frame;
      }
  }
};

class Catch : public Frame
{
private:
  Value _tag;

public:
  Catch(Value tag, Frame * next, Thread * thread)
    : Frame(CATCH, next, thread), _tag(tag)
  {
  }

  Value tag() const
  {
    return _tag;
  }
};

#endif // Frame.hpp
