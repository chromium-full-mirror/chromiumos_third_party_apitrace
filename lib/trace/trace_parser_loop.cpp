/**************************************************************************
 *
 * Copyright 2014 LunarG, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/


#include "trace_parser.hpp"


namespace trace {


// Decorator for parser which loops
class LoopParser : public AbstractParser  {
public:
    LoopParser(AbstractParser *p,
               const FrameSpan &loop_span,
               unsigned long loop_repeat_count)
    : parser(p), loopSpan(loop_span), loopRepeatCount(loop_repeat_count) {}

    ~LoopParser() {
        delete parser;
    }

    Call *parse_call(void) override;

    // Delegate to Parser
    void getBookmark(ParseBookmark &bookmark) override { parser->getBookmark(bookmark); }
    void setBookmark(const ParseBookmark &bookmark) override { parser->setBookmark(bookmark); }
    bool open(const char *filename) override;
    void close(void) override { parser->close(); }
    unsigned long long getVersion(void) const override { return parser->getVersion(); }
    const Properties & getProperties(void) const override { return parser->getProperties(); }
private:
    AbstractParser *parser;
    FrameSpan      loopSpan;
    unsigned long  loopRepeatCount;

    unsigned long  curLoopIteration;
    unsigned long  curFrame;
    bool           frameEnded;
    ParseBookmark  loopStartFrameBookmark;
};

bool
LoopParser::open(const char *filename)
{
    bool ret = parser->open(filename);
    if (ret) {
        curFrame = 0;
        curLoopIteration = 0;
        frameEnded = true;
    }

    return ret;
}

Call *
LoopParser::parse_call(void)
{
    trace::Call *call;
    call = parser->parse_call();

    if (call) {
        if (frameEnded) {
            ++curFrame;
            frameEnded = false;
            if (loopSpan.end && curFrame > loopSpan.end) {
                if (curLoopIteration == loopRepeatCount) {
                    return NULL;
                }
                curFrame = loopSpan.begin;
                parser->setBookmark(loopStartFrameBookmark);
                call = parser->parse_call();
            }
            if (curFrame == loopSpan.begin) {
                if (curLoopIteration == 0) {
                    parser->getBookmark(loopStartFrameBookmark);
                }
                ++curLoopIteration;
            }
        }
        if (call->flags & trace::CALL_FLAG_END_FRAME) {
            frameEnded = true;
        }
    } else {
        if (loopRepeatCount == 0 || curLoopIteration < loopRepeatCount) {
            if (curLoopIteration == loopRepeatCount) {
                return NULL;
            }
            curFrame = loopSpan.begin;
            parser->setBookmark(loopStartFrameBookmark);
            call = parser->parse_call();
        }
    }

    return call;
}


AbstractParser *
loopParser(AbstractParser *parser,
           const FrameSpan &loop_span,
           unsigned long loop_repeat_count)
{
    return new LoopParser(parser, loop_span, loop_repeat_count);
}


} /* namespace trace */
