#include "org/minima/kissvm/tokens/lexical_tokenizer.hpp"
#include "org/minima/kissvm/exceptions/minima_parse_exception.hpp"

#include <iostream>

namespace org {
namespace minima {
namespace kissvm {
namespace tokens {

using org::minima::kissvm::exceptions::MinimaParseException;

//  Constructor now copies tokens by value
LexicalTokenizer::LexicalTokenizer(const std::vector<ScriptToken>& zTokens, int zStackDepth)
    : mTokens(zTokens),  // Make a copy of the tokens - this ensures lifetime safety
      mPos(0),
      mSize(static_cast<int>(zTokens.size())),
      mStackDepth(zStackDepth)  // Use the provided stack depth
{
    // std::cerr << "DEBUG_LT: LexicalTokenizer created with " << mSize << " tokens, stackDepth=" << mStackDepth << "\n" << std::flush;
}

const ScriptToken& LexicalTokenizer::getNextToken() {
    if (mPos >= mSize) {
        // std::cerr << "DEBUG_LT: ERROR - Run out of tokens at position " << mPos << "/" << mSize << "\n" << std::flush;
        throw MinimaParseException("Run out of tokens!..");
    }
    
    //  Direct access to token (no pointer dereferencing needed)
    const ScriptToken& tok = mTokens[mPos];
    
    // std::cerr << "DEBUG_LT: getNextToken[" << mPos << "] type=" << tok.getTokenType() 
    //           << " value=[" << tok.getToken() << "]\n" << std::flush;
    
    mPos++;
    return tok;
}

int LexicalTokenizer::getCurrentPosition() const {
    return mPos;
}

void LexicalTokenizer::goBackToken() {
    if (mPos == 0) {
        // std::cerr << "DEBUG_LT: ERROR - Cannot go back, already at position 0\n" << std::flush;
        throw MinimaParseException("LexicalTokenizer cannot go back as at 0 position");
    }
    mPos--;
    // std::cerr << "DEBUG_LT: goBackToken to position " << mPos << "\n" << std::flush;
}

bool LexicalTokenizer::checkAllTokensUsed() const {
    bool allUsed = (mPos == mSize);
    // std::cerr << "DEBUG_LT: checkAllTokensUsed: " << mPos << "/" << mSize << " = " << (allUsed ? "true" : "false") << "\n" << std::flush;
    return allUsed;
}

bool LexicalTokenizer::hasMoreElements() const {
    bool hasMore = (mPos < mSize);
    // std::cerr << "DEBUG_LT: hasMoreElements: " << mPos << "/" << mSize << " = " << (hasMore ? "true" : "false") << "\n" << std::flush;
    return hasMore;
}

int LexicalTokenizer::getStackDepth() const {
    return mStackDepth;
}

void LexicalTokenizer::incrementStackDepth() {
    mStackDepth++;
    // std::cerr << "DEBUG_LT: Stack depth incremented to " << mStackDepth << "\n" << std::flush;
}

void LexicalTokenizer::decrementStackDepth() {
    mStackDepth--;
    // std::cerr << "DEBUG_LT: Stack depth decremented to " << mStackDepth << "\n" << std::flush;
}

} // namespace tokens
} // namespace kissvm
} // namespace minima
} // namespace org