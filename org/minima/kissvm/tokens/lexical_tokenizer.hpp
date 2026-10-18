#pragma once

#include <vector>
#include "org/minima/kissvm/tokens/script_token.hpp"

namespace org {
namespace minima {
namespace kissvm {
namespace tokens {

/**
 * LexicalTokenizer - FIXED VERSION
 * Now stores tokens by VALUE instead of by POINTER
 * This prevents dangling pointer issues
 */
class LexicalTokenizer {
private:
    // Store actual tokens, not pointers
    std::vector<ScriptToken> mTokens;
    
    int mPos;
    int mSize;
    int mStackDepth;

public:
    /**
     * Constructor now accepts tokens by const reference
     * and makes a copy to ensure lifetime safety
     */
    LexicalTokenizer(const std::vector<ScriptToken>& zTokens, int zStackDepth);
    
    /**
     * Get the next token in the sequence
     * @return Reference to the next token
     * @throws MinimaParseException if no more tokens
     */
    const ScriptToken& getNextToken();
    
    /**
     * Get the current position in the token stream
     */
    int getCurrentPosition() const;
    
    /**
     * Go back one token
     * @throws MinimaParseException if at position 0
     */
    void goBackToken();
    
    /**
     * Check if all tokens have been consumed
     */
    bool checkAllTokensUsed() const;
    
    /**
     * Check if there are more tokens available
     */
    bool hasMoreElements() const;
    
    /**
     * Get the current stack depth
     */
    int getStackDepth() const;
    
    /**
     * Increment the stack depth counter
     */
    void incrementStackDepth();
    
    /**
     * Decrement the stack depth counter
     */
    void decrementStackDepth();
};

} // namespace tokens
} // namespace kissvm
} // namespace minima
} // namespace org