//$Id: TextParser.cpp 11150 2012-12-07 16:55:20Z djcinsb $
//-------------------------------------------------------------------------------
//                              TextParser
//-------------------------------------------------------------------------------
// GMAT: General Mission Analysis Tool
//
// Copyright (c) 2002-2011 United States Government as represented by the
// Administrator of The National Aeronautics and Space Administration.
// All Other Rights Reserved.
//
// Developed jointly by NASA/GSFC and Thinking Systems, Inc. under contract
// number S-67573-G
//
// Author: Linda Jun
// Created: 2006/08/10
//
/**
 * Implements the methods to parse text into parts.
 */
//-------------------------------------------------------------------------------

#include "TextParser.hpp"
#include "StringTokenizer.hpp"
#include "StringUtil.hpp"
#include "UtilityException.hpp"
#include "MessageInterface.hpp"

#include <algorithm>          // Required for GCC 4.3
#include <iterator>           // For back_inserter() with VC++ 2010
#include <stdio.h>            // for sprintf()

//#define DEBUG_TP 1
//#define DEBUG_TP_EVAL_BLOCK 2
//#define DEBUG_TP_CHUNK_LINE 1
//#define DEBUG_TP_DECOMPOSE 2
//#define DEBUG_TP_SEP_BRACKETS 2
//#define DEBUG_TP_DECOMPOSE_BLOCK 1
//#define GMAT_TP_SEPARATEBY

using namespace GmatStringUtil;

//----------------------------------
// public
//----------------------------------

//-------------------------------------------------------------------------------
// TextParser()
//-------------------------------------------------------------------------------
TextParser::TextParser()
{
   whiteSpace = " \t";
}


//-------------------------------------------------------------------------------
// ~TextParser()
//-------------------------------------------------------------------------------
TextParser::~TextParser()
{
}


//-------------------------------------------------------------------------------
// void Initialize(const StringArray &commandList)
//-------------------------------------------------------------------------------
void TextParser::Initialize(const StringArray &commandList)
{
   theCommandList = commandList;
   Reset();
}


//-------------------------------------------------------------------------------
// void Reset()
//-------------------------------------------------------------------------------
/**
 * Resets internal comments and instruction.
 */
//-------------------------------------------------------------------------------
void TextParser::Reset()
{
   prefaceComment = "";
   inlineComment  = "";
   theInstruction = "";
   isFunctionCall = false;
}


//-------------------------------------------------------------------------------
// StringArray DecomposeBlock(const std::string &logicalBlock)
//-------------------------------------------------------------------------------
/*
 * Decomposes logical block into string array of lines.
 * This method expects to end-of-line character for each line since it looks
 * for \n or \r.
 */
//-------------------------------------------------------------------------------
StringArray TextParser::DecomposeBlock(const std::string &logicalBlock)
{
   Integer length = logicalBlock.size();

   #if DEBUG_TP_DECOMPOSE_BLOCK
   MessageInterface::ShowMessage
      ("TextParser::DecomposeBlock() length=%d\n", length);
   #endif

   std::string str = logicalBlock;

   // Replace tabs with spaces to make parsing simpler
   std::replace(str.begin(), str.end(), '\t', ' ');
   length = str.length();

   std::string block;
   Integer lastPos = 0;
   Integer lineCounter = 0;
   StringArray lines;

   // put logicalBlock into StringArray, ending LF or CR
   for (int i=0; i<length; i++)
   {
      #if DEBUG_TP_DECOMPOSE_BLOCK > 1
      MessageInterface::ShowMessage
         ("   ===> TextParser::DecomposeBlock() str[%d]=%c, %d\n", i, str[i], str[i]);
      #endif

      if (str[i] == '\n' || str[i] == '\r')
      {
         // Remove end-of-line character
         block = str.substr(lastPos, i-lastPos+1);
         lines.push_back(block);

         #if DEBUG_TP_DECOMPOSE_BLOCK > 1
         MessageInterface::ShowMessage
            ("   ===> TextParser::DecomposeBlock() i=%d, lastPos=%d, block=%s\n",
             i, lastPos, block.c_str());
         #endif

         lastPos = i+1;
         lineCounter++;
      }
   }


   if (lineCounter == 0)
      lines.push_back(str);
   else if (lastPos < length)
      lines.push_back(str.substr(lastPos));

   return lines;
}


//-------------------------------------------------------------------------------
// Gmat::BlockType EvaluateBlock(const std::string &logicalBlock)
//-------------------------------------------------------------------------------
/*
 * Breaks the logical block into three pieces: preface comments, instruction,
 * inline comments. These pieces are stored internally.
 *
 * @note It removes the keyword GMAT and ending semicolon ';'.
 *       It removes leading and and trailing spaces.
 *
 * @param <logicalBlock> logical block to be evaluated
 *
 * @return BlockType of the logical block.
 *
 * The instruction of object definition block is in the following format:
 *       <"Create"  ObjectType  Name1 [Name2 ...]>
 *
 * The instruction of command definition block is in the following format:
 *       <Command CommandExpression>
 *
 * The instruction of assignment block is in the following format:
 *       <Left = Right>
 *       <"" = Right>         <-- for call function returning no output
 *
 */
//-------------------------------------------------------------------------------
Gmat::BlockType TextParser::EvaluateBlock(const std::string &logicalBlock)
{
   Integer length = logicalBlock.size();

   #if DEBUG_TP_EVAL_BLOCK
   MessageInterface::ShowMessage
      ("TextParser::EvaluateBlock() length=%d\n", length);
   #endif

   #if DEBUG_TP_EVAL_BLOCK > 1
   MessageInterface::ShowMessage
      ("TextParser::EvaluateBlock() logicalBlock=\n<<<%s>>>\n", logicalBlock.c_str());
   #endif

   // first break into string array
   StringArray lines = DecomposeBlock(logicalBlock);

   Integer count = lines.size();

   #if DEBUG_TP_EVAL_BLOCK
   MessageInterface::ShowMessage
      ("TextParser::EvaluateBlock() count=%d\n", count);
   #endif

   Integer commentCounter = 0;
   Integer noCommentLine = -1;
   std::string str, keyword, substr;
   std::string::size_type index1, index2, index3, index4;

   Reset();

   #if DEBUG_TP_EVAL_BLOCK > 1
   for (int i=0; i<count; i++)
      MessageInterface::ShowMessage("   lines[%d]=<%s>\n", i, lines[i].c_str());
   #endif

   // first find block type
   for (int i=0; i<count; i++)
   {
      str = lines[i];
      length = str.size();

      // Remove leading blank spaces
      str = GmatStringUtil::Trim(str, GmatStringUtil::LEADING);

      // Remove GMAT keyword
      index1 = str.find("GMAT ");
      if (index1 == 0)
      {
         index2 = str.find_first_of(whiteSpace, index1);
         index3 = str.find_first_not_of(whiteSpace, index2);
         str = str.substr(index3, length-index3+1);
      }

      index1 = str.find_first_not_of(whiteSpace);
      if (index1 != str.npos)
      {
         if (str[index1] == '%' || str[index1] == '\n' || str[index1] == '\r')
         {
            prefaceComment = prefaceComment + str;
            commentCounter++;
            continue;
         }

         // find keyword, and remove semicolon
         index2 = str.find_first_of(whiteSpace, index1);

         //If white space found (Added for VC++)
         if (index2 != str.npos)
         {
            if (str[index2-1] == ';')
               keyword = str.substr(index1, index2-index1-1);
            else
               keyword = str.substr(index1, index2-index1);
         }
         else
         {
            keyword = str.substr(index1);
         }

         // remove any eol or semicolon from keyword
         keyword = GmatStringUtil::Trim(keyword, GmatStringUtil::BOTH, true, true);

         // make sure keyword is before open parenthesis
         std::string::size_type openIndex = keyword.find("(");
         if (openIndex != keyword.npos)
            keyword = keyword.substr(0, openIndex);

         // check for "function"
         if (keyword == "function")
         {
            theBlockType = Gmat::FUNCTION_BLOCK;
            noCommentLine = i;
         }
         // check for "Create" or Commands
         else if (keyword == "Create")
         {
            theBlockType = Gmat::DEFINITION_BLOCK;
            noCommentLine = i;
         }
         else if (IsCommand(keyword))
         {
            theBlockType = Gmat::COMMAND_BLOCK;
            noCommentLine = i;
         }
         else
         {
            theBlockType = Gmat::ASSIGNMENT_BLOCK;
            noCommentLine = i;

            // check for CallFunction
            // ex) [a b c] = function(d, e, f);

            //if (str.find("[") != str.npos) // Is this checking enough?

            // check for RVECTOR_TYPE or UNSIGNED_INTARRAY_TYPE setting
            // ex) opengl.OrbitColor = [100 200 300 ];
            //     opengl.ViewPointVectorVector = [0, 0, 50000];

            std::string::size_type index1 = str.find("[");
            if (index1 != str.npos)
            {
               std::string::size_type index2 = str.find("=");
               if (index2 == str.npos || index2 > index1)
               {
                  theBlockType = Gmat::COMMAND_BLOCK;
                  isFunctionCall = true;
               }
            }

            /// @todo: This is a work around for a call function with
            /// without any return parameters.  It should be updated in
            /// the design to handle this situation.
            std::string::size_type commentPos = str.find("%");
            std::string noInline = str;
            if (commentPos != str.npos)
               noInline = str.substr(0, commentPos);

            // Since we are allowed to pass string literal, string literal can
            // be anything letters including equal sign, so we need additional checking.
            // String literals are enclosed with single quotes (loj: 2008.05.19)
            // See if it has equal sign and math symbols not in quotes.
            if (!GmatStringUtil::IsThereMathSymbol(str))
            {
               theBlockType = Gmat::COMMAND_BLOCK;
               isFunctionCall = true;
            }
         }

         if (noCommentLine >= 0)
         {
            // if % found in the no-comment line, it is inline comment
            index3 = str.find("%", index2);

            #if DEBUG_TP_EVAL_BLOCK
            MessageInterface::ShowMessage
            ("   index1=%d, index2=%u, index3=%u\n", index1, index2, index3);
            #endif

            // if inline comment
            if (index3 != str.npos)
            {
               // find last non-blank
               index4 = str.find_last_not_of(whiteSpace, index3-1);

               // remove eol char from inlineComment
               inlineComment = str.substr(index4+1);
               inlineComment = inlineComment.substr(0, inlineComment.size()-1);

               theInstruction = str.substr(index1, index4-index1+1);
            }
            else
            {
               theInstruction = str.substr(index1);
            }
         }
      }
   }

   //MessageInterface::ShowMessage("===> theInstruction=%s\n", theInstruction.c_str());

   if (commentCounter == count)
      theBlockType = Gmat::COMMENT_BLOCK;

   // remove ending ; from the instruction
   theInstruction =
      GmatStringUtil::Trim(theInstruction, GmatStringUtil::TRAILING, true, true);

   // Replace tabs with spaces in theInstruction
   // Convert tab characters to a single space
   #ifdef DEBUG_WHITESPACE
      MessageInterface::ShowMessage("Before: \"%s\"\n", theInstruction.c_str());
   #endif
   if (theInstruction != "")
   {
      std::replace(theInstruction.begin(), theInstruction.end(), '\t', ' ');

      // Strip off leading and trailing whitespace
      std::string whitespace = " \t";
      Integer strBegin = theInstruction.find_first_not_of(whitespace);
      Integer strEnd = theInstruction.find_last_not_of(whitespace);
      Integer strRange = strEnd - strBegin + 1;
      theInstruction = theInstruction.substr(strBegin, strRange);
   }
   #ifdef DEBUG_WHITESPACE
      MessageInterface::ShowMessage("After:  \"%s\"\n", theInstruction.c_str());
   #endif

   theChunks.clear();
   theChunks.push_back(prefaceComment);
   theChunks.push_back(inlineComment);
   theChunks.push_back(theInstruction);

   #if DEBUG_TP_EVAL_BLOCK
   MessageInterface::ShowMessage
      ("   keyword=<%s>, blockType=%d, isFunctionCall=%d\n", keyword.c_str(),
       theBlockType, isFunctionCall);
   MessageInterface::ShowMessage
      ("   prefaceComment=<%s>\n   inlineComment=<%s>\n   theInstruction=<%s>\n",
       prefaceComment.c_str(), inlineComment.c_str(), theInstruction.c_str());
   #endif

   return theBlockType;
}


//-------------------------------------------------------------------------------
// StringArray ChunkLine()
//-------------------------------------------------------------------------------
/*
 * Breaks the internal instruction string into logical groups.  The instruction
 * line is broken at white space and comma characters. Blocks marked with the
 * grouping delimiters (), {}, and [] are kept together.
 *
 * @note It removes the keyword GMAT and ending semicolon ';'.
 *       It removes leading and and trailing spaces.
 *
 * @return array of constituent parts.
 *    The object definition instructions returns in the following format:
 *       <"Create"> <ObjectType> <Name1> [<Name2> ...]

 *    The command definition instructions returns in the following format:
 *       <Command> <CommandExpression>   <-- command, EndFiniteBurn
 *       <Command>                       <-- End* command
 *
 *    The assignment definition instructions returns in the following format:
 *       <Left> <Right>
 *
 *    The function definition instructions returns in the following format:
 *       <"CallFunction"> <out> <FunctionName> <in>
 *
 * @exception <InterpreterException> thrown if no object type or name found
 *
 *
 * Examples of object definition line are:
 *   Create Spacecraft Sat1 Sat2, Sat3
 *      <Create> <Spacecraft> <Sat1 Sat2, Sat3>
 *   Create Array Mat1[3,3], Mat2[6,3];
 *      <Create> <Array> <Mat1[3,3], Mat2[6,3]>
 *
 * Examples of command definition line are:
 *   ----- Commands:
 *   Save Sat1 Sat2,Sat3
 *      <Save> <Sat1 Sat2,Sat3>
 *   Report reportObject Mat1, Mat2,Mat2(1,1);
 *      <Report> <reportObject Mat1, Mat2,Mat2(1,1)>
 *   Report reportObject Mat2(1,1), Mat2(1,2) Mat2(1,3);
 *      <Report> <reportObject Mat2(1,1), Mat2(1,2) Mat2(1,3)>
 *   BeginFiniteBurn burn1(Sat1);
 *      <BeginFiniteBurn> <burn1(Sat1)>
 *   Propagate prop(Sat1, Sat2, {Sat1.ElapsedDays = 10})
 *      <Propagate> <prop(Sat1, Sat2, {Sat1.ElapsedDays = 10})>
 *   Propagate -prop(Sat1, Sat2, {Sat1.Periapsis})
 *      <Propagate> <-prop(Sat1, Sat2, {Sat1.Periapsis})>
 *   Achieve myDC(Sat1.SMA = 21545.0, {Tolerance = 0.1});
 *      <Achieve> <myDC(Sat1.SMA = 21545.0, {Tolerance = 0.1})>
 *   Achieve myDC(Sat1.SMA = Arr1(I,J), {Tolerance = 0.1});
 *      <Achieve> <myDC(Sat1.SMA = Arr1(I,J), {Tolerance = 0.1})>
 *   Vary DC(DefaultSC.SMA = 6500, {Pert = 1, MaxStep = 1000, Lower = 6000, Upper = 100000});
 *      <Vary> <DC(DefaultSC.SMA = 6500, {Pert = 1, MaxStep = 1000, Lower = 6000, Upper = 100000})>
 *   For step = 1 : 10 : 2
 *      <For> <step = 1 : 10 : 2>
 *   While var1 < Sat1.X
 *      <While> <var1 < Sat1.X>
 *   If var1 ~= var2
 *      <If> <var1 ~= var2>
 *   EndIf
 *      <EndIf>
 *
 *   ----- Function Call:
 *   StoreData( FormState );  <== No output
 *      <> <StoreData( FormState )>
 *   GMAT [S1,S2,S3,S1dot,S2dot,S3dot] = GetLISAData(x, y, z, v(1), vv(1,1), vz ); <== RHS has [
 *      <[S1,S2,S3,S1dot,S2dot,S3dot]> <GetLISAData(x, y, z, v(1), vv(1,1), vz )>
 *
 * Examples of assignment definition line are:
 *   Sat1.X = 7000;
 *      <Sat1.X> <7000>
 *   Sat1.Y = Sat2.Y;
 *      <Sat1.Y> = <Sat2.Y>
 *   Sat1.Z = Mat1(1,3);
 *      <Sat1.Z> = <Mat1(1,3)>
 *   Sat1.VX = var1;
 *      <Sat1.VX> <var1>
 *   Sat1 = Sat2;
 *      <Sat1> <Sat2>
 *   Mat1(1,1) = Sqrt(a + b + c + mat1(1,1)^2)
 *      <Mat1(1,1)> <Sqrt(a + b + c + mat1(1,1)^2)>
 *
 *   ----- Function Call:
 *   var2 = MyFunction(); <== RHS is variable
 *      <var2> <MyFunction()>
 *
 */
//-------------------------------------------------------------------------------
StringArray TextParser::ChunkLine()
{
   #if DEBUG_TP_CHUNK_LINE
   MessageInterface::ShowMessage
      ("TextParser::ChunkLine() theInstruction=<%s>\n   theBlockType=%d, "
       "isFunctionCall=%d\n", theInstruction.c_str(), theBlockType, isFunctionCall);
   #endif

   std::string str = theInstruction;
   std::string space;
   std::string::size_type index1, index2, index3 = 123456;

   Integer length = str.size();
   StringArray chunks;

   //------------------------------------------------------------
   // object definition block
   //------------------------------------------------------------
   if (theBlockType == Gmat::DEFINITION_BLOCK)
   {
      // find keyword Create
      index1 = str.find("Create");
      if (index1 == str.npos)
      {
         #if DEBUG_TP_CHUNK_LINE
         sprintf(errorMsg, "TextParser::ChunkLine() keyword \"Create\" not "
                 "found in the definition block\n   \"%s\"\n", str.c_str());
         MessageInterface::ShowMessage("%s", errorMsg);
         #endif

         sprintf(errorMsg, "The keyword \"Create\" not found in the definition block");
         throw UtilityException(errorMsg);
      }
      else
      {
         // find object type
         index2 = str.find_first_of(whiteSpace, index1);
         chunks.push_back(str.substr(index1, index2-index1));

         index1 = str.find_first_not_of(whiteSpace, index2);

         if (index1 == str.npos)
         {
            #if DEBUG_TP_CHUNK_LINE
            sprintf(errorMsg, "TextParser::ChunkLine() object type not "
                    "found in the definition block\n   \"%s\"\n", str.c_str());
            MessageInterface::ShowMessage("%s", errorMsg);
            #endif

            sprintf(errorMsg, "Object type not found in the definition block");
            throw UtilityException(errorMsg);
         }
         else
         {
            index2 = str.find_first_of(whiteSpace, index1);
            chunks.push_back(str.substr(index1, index2-index1));

            index1 = str.find_first_not_of(whiteSpace, index2);

            if (index1 == str.npos)
            {
               #if DEBUG_TP_CHUNK_LINE
               sprintf(errorMsg, "TextParser::ChunkLine() object name not "
                       "found in the definition block\n   \"%s\"\n", str.c_str());
               MessageInterface::ShowMessage("%s", errorMsg);
               #endif

               sprintf(errorMsg, "Object name not found in the definition block");
               throw UtilityException(errorMsg);
            }
            else
            {
               chunks.push_back(str.substr(index1, length-index1+1));
            }
         }
      }
   }
   //------------------------------------------------------------
   // command block
   //------------------------------------------------------------
   else if (theBlockType == Gmat::COMMAND_BLOCK && !isFunctionCall)
   {
      index1 = str.find_first_not_of(whiteSpace);
      if (index1 == str.npos)
      {
         #if DEBUG_TP_CHUNK_LINE
         sprintf(errorMsg, "TextParser::ChunkLine() command name not found "
                 "in the command block\n   \"%s\"\n", str.c_str());
         MessageInterface::ShowMessage("%s", errorMsg);
         #endif

         sprintf(errorMsg, "Command name not found in the command block");
         throw UtilityException(errorMsg);
      }
      else
      {
         index2 = str.find_first_of(whiteSpace, index1);

         if (index2 == str.npos)
         {
            chunks.push_back(str);
         }
         else
         {
            chunks.push_back(str.substr(index1, index2-index1));

            index1 = str.find_first_not_of(whiteSpace, index2);

            if (index1 != str.npos)
               chunks.push_back(str.substr(index1, length-index1));
         }
      }
   }
   //------------------------------------------------------------
   // assignment block
   //------------------------------------------------------------
   else if (theBlockType == Gmat::ASSIGNMENT_BLOCK || isFunctionCall)
   {
      // find equal sign
      index1 = str.find_first_not_of(whiteSpace);

      if (index1 == str.npos)
      {
         #if DEBUG_TP_CHUNK_LINE
         sprintf(errorMsg, "TextParser::ChunkLine() no assignment expression "
                 "found in the assignment block\n   \"%s\"\n", str.c_str());
         MessageInterface::ShowMessage("%s", errorMsg);
         #endif

         sprintf(errorMsg, "No assignment expression found in the assignment block");
         throw UtilityException(errorMsg);
      }
      else
      {
         index2 = str.find_first_of("=", index1);

         if (index2 == str.npos)
         {
            // set "" to lhs
            chunks.push_back("");
            chunks.push_back(str);
         }
         else
         {
            if (index2 != 0)
               index3 = str.find_last_not_of(whiteSpace, index2-1);

            #if DEBUG_TP_CHUNK_LINE
            MessageInterface::ShowMessage("   index3=%u, index2=%u\n", index3, index2);
            #endif

            if (index3 == str.npos || index2 == 0)
            {
               #if DEBUG_TP_CHUNK_LINE
               sprintf(errorMsg, "TextParser::ChunkLine() LHS of = not found in "
                       "the assignment block\n   \"%s\"\n", str.c_str());
               MessageInterface::ShowMessage("%s", errorMsg);
               #endif

               sprintf(errorMsg, "LHS of \"=\" not found in the assignment block");
               throw UtilityException(errorMsg);
            }
            else
            {
               // found lhs
               chunks.push_back(str.substr(index1, index3-index1+1));
               index1 = str.find_first_not_of(whiteSpace, index2+1);

               if (index1 == str.npos)
               {
                  #if DEBUG_TP_CHUNK_LINE
                  sprintf(errorMsg, "TextParser::ChunkLine() RHS of = not found in "
                          "the assignment block\n   \"%s\"\n", str.c_str());
                  MessageInterface::ShowMessage("%s", errorMsg);
                  #endif

                  sprintf(errorMsg, "RHS of \"=\" not found in the assignment block");
                  throw UtilityException(errorMsg);
               }
               else
               {
                  chunks.push_back(str.substr(index1, length-index1));
               }
            }
         }
      }
   }

   #if DEBUG_TP_CHUNK_LINE
   MessageInterface::ShowMessage("   Returning:\n");
   for (unsigned int i=0; i<chunks.size(); i++)
      MessageInterface::ShowMessage("   chunks[%d]=%s\n", i, chunks[i].c_str());
   #endif

   return chunks;
}


//-------------------------------------------------------------------------------
// StringArray Decompose(const std::string &chunk, const std::string &bracketPair
//                       bool checkForArray = true, bool removeOuterBracket = false)
//-------------------------------------------------------------------------------
/*
 * Breaks chunk into parts separated by space or comma but keeps bracket together.
 *
 * @param  chunk  Input string to be broken into compoments
 * @param  bracketPair  The bracket pair used for breaking the string
 * @param  checkForArray  true if check for valid array form (true)
 * @param  removeOuterBracket  true if requesting outer bracket to be removed (false)
 *
 * For example:
 * string = "BeginFiniteBurn burn1(sat1 sat2)"
 * Decomose(string, false, false) will return
 *    <burn1> <sat1 sat2>
 *
 * string = "DC(DefaultSC.SMA=6500,{Pert=1,MaxStep=1000,Lower=6000,Upper=100000})"
 * Decompose(string, true, true) will return
 *    <DC> <(DefaultSC.SMA=6500,{Pert=1,MaxStep=1000,Lower=6000,Upper=100000})
 *
 * string = "DC(DefaultSC.SMA=Vec(3),{Pert=1,MaxStep=1000,Lower=6000,Upper=100000})"
 * Decompose(string, true, true) will return
 *    <DC> <(DefaultSC.SMA=Vec(3),{Pert=1,MaxStep=1000,Lower=6000,Upper=100000})
 *
 * string = "(DefaultSC.SMA=6500,{Pert=1,MaxStep=1000,Lower=6000,Upper=100000})"
 * Decompose(string, true, true) will return
 *    <DefaultSC.SMA=6500> <{Pert=1,MaxStep=1000,Lower=6000,Upper=100000}>
 *
 * string = "(DefaultSC.SMA=Vec(3),{Pert=1,MaxStep=1000,Lower=6000,Upper=100000})"
 * Decompose(string, true, true) will return
 *    <DefaultSC.SMA=Vec(3)> <{Pert=1,MaxStep=1000,Lower=6000,Upper=100000}>
 *
 * string = "(DefaultSC.SMA=G(1,1),{Pert=P(1,1),MaxStep=M(1,1),Lower=L(1,1),Upper=U(1,1)})"
 * Decompose(string, true, true) will return
 *    <DefaultSC.SMA=G(1,1)> <{Pert=P(1,1),MaxStep=M(1,1),Lower=L(1,1),Upper=U(1,1)}>
 *
 * @param <chunk>  Input chunk to be break apart
 * @param <bracketPair>  Input bracket pair (open and close) to keep together
 * @param <checkForArray>  Check for array if it is true (ie. a(i,j), v(1))
 * @param <removeOuterBracket>  Removes outer bracket if it is true
 */
//-------------------------------------------------------------------------------
StringArray TextParser::Decompose(const std::string &chunk,
                                  const std::string &bracketPair,
                                  bool checkForArray,
                                  bool removeOuterBracket)
{
   #if DEBUG_TP_DECOMPOSE
   MessageInterface::ShowMessage
      ("TextParser::Decompose() chunk='%s', bracketPair='%s', checkForArray=%d, "
       "removeOuterBracket=%d\n", chunk.c_str(), bracketPair.c_str(), checkForArray,
       removeOuterBracket);
   #endif
   
   std::string str1 = chunk;

   // If checking for array, first remove blank spaces inside array bracket
   if (checkForArray && chunk[0] != bracketPair[0])
      str1 = RemoveSpaceInBrackets(chunk, bracketPair);

   int length = str1.size();
   std::string::size_type openBracketIndex;
   Integer open, close;
   bool isOuterBracket = false;
   
   // Remove outer bracket if flag is set to true
   openBracketIndex = str1.find_first_of(bracketPair[0]);
   if (openBracketIndex == 0)
   {
      GmatStringUtil::FindMatchingBracket(str1, open, close, isOuterBracket,
                                          bracketPair, 0);

      #if DEBUG_TP_DECOMPOSE
      MessageInterface::ShowMessage
         ("   open=%d, close=%d, isOuterBracket=%d\n", open, close,
          isOuterBracket);
      #endif

      if (removeOuterBracket && isOuterBracket)
      {
         str1 = str1.substr(1, length-2);

         #if DEBUG_TP_DECOMPOSE
         MessageInterface::ShowMessage("   str1=%s\n", str1.c_str());
         #endif
      }
   }
   
   StringArray parts;
   std::string::size_type openBrace = str1.find_first_of("{");
   std::string::size_type closeBrace = str1.find_last_of("}");
   
   //-----------------------------------------------------------------
   // check for brace first to simplify decompose
   //-----------------------------------------------------------------
   if (openBrace != str1.npos && closeBrace != str1.npos)
   {
      UnsignedInt firstComma = str1.find_last_of(",", openBrace);
      ////loj:UnsignedInt firstComma = str1.find_last_of(", ", openBrace);
      
      if (closeBrace == str1.size()-1)
      {
//          if (firstComma != str1.npos)
//             parts.push_back(str1.substr(0, firstComma));
//          else
//             parts.push_back(str1.substr(0, openBrace));
         parts.push_back(str1.substr(0, firstComma));
         parts.push_back(str1.substr(openBrace));
         
         #if DEBUG_TP_DECOMPOSE
         MessageInterface::ShowMessage("   Returning:\n");
         for (unsigned int i=0; i<parts.size(); i++)
            MessageInterface::ShowMessage
               ("   parts[%d] = %s\n", i, parts[i].c_str());
         #endif
         
         return parts;
      }
   }
   
   
   //-----------------------------------------------------------------
   // now separate by space and comma and put brackets together
   //-----------------------------------------------------------------
   std::string openBrackets = "([{";
   openBracketIndex = str1.find_first_of(openBrackets);

   bool hasOpenBracket = false;
   bool isArray = false;

   // if open bracket found,
   if (openBracketIndex != str1.npos)
   {
      bool isBracketPartOfArray = false;
      hasOpenBracket = true;
      isArray = true;

      GmatStringUtil::FindMatchingBracket(str1, open, close, isOuterBracket,
                                          bracketPair, openBracketIndex);

      #if DEBUG_TP_DECOMPOSE > 1
      MessageInterface::ShowMessage
         ("   open=%d, close=%d, isOuterBracket=%d\n", open, close, isOuterBracket);
      #endif

      if (checkForArray)
      {
         if (open == -1 && close == -1)
            isBracketPartOfArray =
               IsBracketPartOfArray(str1.substr(openBracketIndex), "([)]", true);
         else
            isBracketPartOfArray =
               IsBracketPartOfArray(str1.substr(open, close-open+1), "([)]", true);
      }

      #if DEBUG_TP_DECOMPOSE > 1
      MessageInterface::ShowMessage("   isBracketPartOfArray=%d\n", isBracketPartOfArray);
      #endif

      if (!isBracketPartOfArray)
         isArray = false;
   }

   #if DEBUG_TP_DECOMPOSE > 1
   MessageInterface::ShowMessage
      ("   hasOpenBracket=%d, isArray=%d\n", hasOpenBracket, isArray);
   #endif

   // if no Array found, just separate by comma and space
   if (!hasOpenBracket && !isArray)
   {
      parts = GmatStringUtil::SeparateBy(str1, ", ", true);
   }
   else if (hasOpenBracket && !isArray)
   {
      if (openBracketIndex != 0)
      {
         std::string str2 = str1.substr(0, openBracketIndex);
         str2 = GmatStringUtil::Trim(str2);
         parts.push_back(str2);
      }

      parts.push_back(str1.substr(openBracketIndex, length-openBracketIndex));

   }
   else
   {
      parts = GmatStringUtil::SeparateBy(str1, " ", true);
   }

   #if DEBUG_TP_DECOMPOSE
   MessageInterface::ShowMessage("   Returning: %d parts\n", parts.size());
   for (unsigned int i=0; i<parts.size(); i++)
      MessageInterface::ShowMessage("   parts[%d] = %s\n", i, parts[i].c_str());
   #endif

   return parts;
}


//-------------------------------------------------------------------------------
// StringArray SeparateBrackets(const std::string &chunk,
//                              const std::string &bracketPair,
//                              const std::string &delim,
//                              bool checkOuterBracket)
//-------------------------------------------------------------------------------
/*
 * Breaks chunk into parts separated by space or comma but keeps bracket together,
 * except outer most brackets.
 *
 * For example:
 * string = "{Pert=P(1,1),MaxStep=M(1,1),Lower=L(1,1),Upper=U(1,1)}"
 * SeparateBrackets(string, "{}", ",", false) will return
 *    <Pert=P(1,1)> <MaxStep=M(1,1)> <Lower=L(1,1)> <Upper=U(1,1)>
 *
 * @param <chunk> input chunk to be break apart
 * @param <bracketPair> input bracket pair (open and close) to keep together
 *                      (), [], {}
 * @param <checkOuterBracket> true if outer bracket pair must be exist (true)
 *
 * @exception <InterpreterException> thrown
 *    If checkOuterBracket is set to true, and there is no matching bracket pair
 */
//-------------------------------------------------------------------------------
StringArray TextParser::SeparateBrackets(const std::string &chunk,
                                         const std::string &bracketPair,
                                         const std::string &delim,
                                         bool checkOuterBracket)
{
   #if DEBUG_TP_SEP_BRACKETS
   MessageInterface::ShowMessage
      ("TextParser::SeparateBrackets() chunk='%s', bracketPair='%s', delim='%s', "
       "checkOuterBracket=%d\n", chunk.c_str(), bracketPair.c_str(), delim.c_str(),
       checkOuterBracket);
   #endif
   
   std::string str1 = chunk;
   
   // First remove blank spaces inside array bracket
   if (chunk[0] != bracketPair[0])
      str1 = RemoveSpaceInBrackets(chunk, bracketPair);
   
   #if DEBUG_TP_SEP_BRACKETS
   MessageInterface::ShowMessage("   str1=%s\n", str1.c_str());
   #endif
   
   UnsignedInt firstOpen, lastClose;
   firstOpen = str1.find_first_not_of(whiteSpace);
   lastClose = str1.find_last_not_of(whiteSpace);
   bool bracketFound = true;
   
   if (str1[firstOpen] != bracketPair[0] || str1[lastClose] != bracketPair[1])
   {
      bracketFound = false;
      if (checkOuterBracket)
      {
         sprintf(errorMsg, "TextParser::SeparateBrackets() \"%s\" is not enclosed "
                 "with \"%s\"", str1.c_str(), bracketPair.c_str());

         #if DEBUG_TP_SEP_BRACKETS
         MessageInterface::ShowMessage("%s\n", errorMsg);
         #endif

         sprintf(errorMsg, "\"%s\" is not enclosed with \"%s\"", str1.c_str(),
                 bracketPair.c_str());
         throw UtilityException(errorMsg);
      }
   }
   
   std::string str;
   
   if (bracketFound)
      str = str1.substr(firstOpen+1, lastClose-firstOpen-1);
   else
      str = str1.substr(firstOpen, lastClose-firstOpen+1);
   
   #if DEBUG_TP_SEP_BRACKETS
   MessageInterface::ShowMessage("   str=%s\n", str.c_str());
   #endif
   
   
   StringArray parts;
   parts = GmatStringUtil::SeparateBy(str, delim, true);
   
   #if DEBUG_TP_SEP_BRACKETS
   MessageInterface::ShowMessage("   Returning:\n");
   for (unsigned int i=0; i<parts.size(); i++)
      MessageInterface::ShowMessage
         ("   parts[%d] = %s\n", i, parts[i].c_str());
   #endif
   
   return parts;
}


//-------------------------------------------------------------------------------
// StringArray SeparateAllBrackets(const std::string &chunk)
//-------------------------------------------------------------------------------
StringArray TextParser::SeparateAllBrackets(const std::string &chunk,
                                            const std::string &bracketPair)
{
   #if DEBUG_TP_SEP_BRACKETS
   MessageInterface::ShowMessage
      ("TextParser::SeparateAllBrackets() chunk='%s', bracketPair='%s'\n",
       chunk.c_str(), bracketPair.c_str());
   #endif
   
   std::string str1 = chunk;
   std::string parenPair = "()";
   std::string openBracket, closeBracket;
   openBracket = bracketPair[0];
   if (bracketPair.size() > 1)
      closeBracket = bracketPair[1];
   
   // First remove blank spaces inside array bracket
   if (chunk[0] != parenPair[0])
      str1 = RemoveSpaceInBrackets(chunk, parenPair);
   
   #if DEBUG_TP_SEP_BRACKETS
   MessageInterface::ShowMessage("   str1=%s\n", str1.c_str());
   #endif
   
   StringArray parts, parts1, parts2;
   parts1 = GmatStringUtil::SeparateBy(str1, "}", false, true);
   
   for (Integer i = 0; i < (Integer)parts1.size(); i++)
   {
      #if DEBUG_TP_SEP_BRACKETS
      MessageInterface::ShowMessage
         ("   parts1[%d] = %s\n", i, parts1[i].c_str());
      #endif
      parts2 = GmatStringUtil::SeparateBy(parts1[i], "{", false, false);
      for (unsigned int j=0; j<parts2.size(); j++)
      {
         #if DEBUG_TP_SEP_BRACKETS
         MessageInterface::ShowMessage
            ("   parts2[%d] = %s\n", j, parts2[j].c_str());
         #endif
         // If closeBracket found at the end, insert openBracket back if not found
         if ((parts2[j][0] != '{') &&
             (parts2[j].find(closeBracket) == parts2[j].size()-1))
            parts2[j].insert(0, openBracket);
      }
      copy(parts2.begin(), parts2.end(), back_inserter(parts));
   }
   
   #if DEBUG_TP_SEP_BRACKETS
   MessageInterface::ShowMessage
      ("TextParser::SeparateAllBrackets() returning %d parts:\n", parts.size());
   for (unsigned int i=0; i<parts.size(); i++)
      MessageInterface::ShowMessage
         ("   parts[%d]  = %s\n", i, parts[i].c_str());
   #endif
   
   return parts;
}


//-------------------------------------------------------------------------------
// StringArray SeparateSpaces(const std::string &chunk)
//-------------------------------------------------------------------------------
/*
 * Breaks string by space or comma.
 *
 * @param <chunk> input string to be break apart
 *
 * @return string array of parts
 */
//-------------------------------------------------------------------------------
StringArray TextParser::SeparateSpaces(const std::string &chunk)
{
   StringTokenizer st(chunk, " ,\t");
   StringArray parts = st.GetAllTokens();

   #if DEBUG_SEP_SPACE
   for (UnsignedInt i=0; i<parts.size(); i++)
      MessageInterface::ShowMessage
         ("   parts[%d]=%s\n", i, parts[i].c_str());
   #endif

   return parts;
}


//-------------------------------------------------------------------------------
// StringArray SeparateDots(const std::string &chunk)
//-------------------------------------------------------------------------------
/*
 * Breaks string by dots, but keep decimal point number together.
 *
 * @param <chunk> input string to be break apart
 *
 * @return string array of parts
 */
//-------------------------------------------------------------------------------
StringArray TextParser::SeparateDots(const std::string &chunk)
{
   Real rval;
   StringArray parts;

   // Separate by dots if not a number
   if (GmatStringUtil::ToReal(chunk, rval))
   {
      parts.push_back(chunk);
   }
   else
   {
      StringTokenizer st(chunk, ".");
      parts = st.GetAllTokens();
   }

   #if DEBUG_SEP_DOTS
   for (UnsignedInt i=0; i<parts.size(); i++)
      MessageInterface::ShowMessage
         ("   parts[%d]=%s\n", i, parts[i].c_str());
   #endif

   return parts;
}


//-------------------------------------------------------------------------------
// StringArray SeparateBy(const std::string &chunk, const std::string &delim)
//-------------------------------------------------------------------------------
/*
 * Breaks string by input delimiter. Removes leading and trailing spaces.
 *
 * @param <chunk> input string to be break apart
 * @param <delim> input delimiter used in breaking apart
 *
 * @return string array of parts
 */
//-------------------------------------------------------------------------------
StringArray TextParser::SeparateBy(const std::string &chunk, const std::string &delim)
{
   #ifdef GMAT_TP_SEPARATEBY
   MessageInterface::ShowMessage
      ("TextParser::SeparateBy() chunk='%s', delim='%s'\n", chunk.c_str(), delim.c_str());
   #endif
   
   StringTokenizer st(chunk, delim);
   StringArray parts = st.GetAllTokens();
   int count = parts.size();
   
   for (int i=0; i<count; i++)
      parts[i] = GmatStringUtil::Strip(parts[i], GmatStringUtil::BOTH);
   
   #ifdef GMAT_TP_SEPARATEBY
   MessageInterface::ShowMessage
      ("TextParser::SeparateBy() returning:\n");
   for (int i=0; i<count; i++)
      MessageInterface::ShowMessage
         ("   parts[%d]=%s\n", i, parts[i].c_str());
   #endif
   
   return parts;
}


//-------------------------------------------------------------------------------
// bool IsCommand(const std::string &str)
//-------------------------------------------------------------------------------
bool TextParser::IsCommand(const std::string &str)
{
   bool found = false;

   if (find(theCommandList.begin(), theCommandList.end(), str)
       != theCommandList.end())
      found = true;

   return found;
}


//-------------------------------------------------------------------------------
// char GetClosingBracket(const char &openBracket)
//-------------------------------------------------------------------------------
char TextParser::GetClosingBracket(const char &openBracket)
{
   switch (openBracket)
   {
   case '(':
      return ')';
   case '[':
      return ']';
   case '{':
      return '}';
   case '<':
      return '>';

   default:
      sprintf(errorMsg, "TextParser found unknown open bracket: %c", openBracket);
      throw UtilityException(errorMsg);
   }
}

