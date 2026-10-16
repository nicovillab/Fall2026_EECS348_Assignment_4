/*
 * Program name : EECS 348 Assignment 4 - Recursive Object-Oriented Sudoku Solver
 * Description  : Reads five Sudoku puzzles from text files and finds every
 *                solution to each one using a recursive depth-first search with
 *                backtracking.
 * Inputs       : puzzle1.txt, puzzle2.txt, puzzle3.txt, puzzle4.txt, puzzle5.txt
 *                (9 lines of 9 space-separated entries; 1-9 = given digit,
 *                "_" = blank cell), read from the current working directory.
 * Output       : For each file: the file name, the puzzle as read, every
 *                solution found (or "No solution found"), and the solution count.
 * Collaborators and other sources:
 *                No human collaborators.
 *                Claude Opus 5.5 (claude.ai): baseline program and the revisions
 *                described in Assignment4GenAI.pdf, section g.
 * Author       : Nicolas Villalobos
 * Created      : October 15, 2026
 * Revised      : October 15, 2026
 */

#include <fstream>                                       // std::ifstream to read puzzle files
#include <iostream>                                      // std::cout / std::ostream for output
#include <string>                                        // std::string for names and tokens
#include <vector>                                        // std::vector for the file-name list

// ---------------------------------------------------------------------------
// SudokuBoard: stores one 9x9 grid (0 = blank), loads it, and prints it.
// Source: Claude baseline, revised.
// ---------------------------------------------------------------------------
class SudokuBoard {                                      // grid storage and file I/O
public:                                                  // members usable by other classes
    static const int SIZE = 9;                           // rows and columns in the grid
    static const int BOX = 3;                            // side length of one 3x3 block
    static const int EMPTY = 0;                          // value stored for a blank cell

    SudokuBoard() {                                      // constructor: start all blank
        for (int r = 0; r < SIZE; ++r)                   // visit every row
            for (int c = 0; c < SIZE; ++c)               // visit every column of that row
                cells[r][c] = EMPTY;                     // mark the cell blank
    }                                                    // end constructor

    bool loadFromFile(const std::string& fileName, std::string& error) {  // read 81 entries; false on error
        std::ifstream in(fileName.c_str());              // open the puzzle file
        if (!in) {                                       // file missing or unreadable
            error = "Could not open file";               // record the reason
            return false;                                // report failure
        }                                                // end open check
        for (int r = 0; r < SIZE; ++r) {                 // read row by row
            for (int c = 0; c < SIZE; ++c) {             // read each entry in the row
                std::string token;                       // holds one entry from the file
                if (!(in >> token)) {                    // fewer than 81 entries
                    error = "File ended early (expected 81 entries)";  // record the reason
                    return false;                        // cannot build a full grid
                }                                        // end read check
                if (token == "_") {                      // underscore means blank
                    cells[r][c] = EMPTY;                 // store blank as 0
                } else if (token.size() == 1 && token[0] >= '1' && token[0] <= '9') {  // single digit 1-9
                    cells[r][c] = token[0] - '0';        // convert character to integer
                } else {                                 // anything else is invalid
                    error = "Invalid entry '" + token + "' at row " +  // start the message
                            std::to_string(r + 1) + ", column " + std::to_string(c + 1);  // add position
                    return false;                        // reject the file
                }                                        // end token check
            }                                            // end column loop
        }                                                // end row loop
        return true;                                     // all 81 entries read
    }                                                    // end loadFromFile

    int get(int row, int col) const { return cells[row][col]; }        // read one cell
    void set(int row, int col, int value) { cells[row][col] = value; } // write one cell

    void print(std::ostream& out) const {                // print in the input file's layout
        for (int r = 0; r < SIZE; ++r) {                 // one output line per row
            for (int c = 0; c < SIZE; ++c) {             // each cell in the row
                if (cells[r][c] == EMPTY) out << '_';    // blank cell shown as _
                else out << cells[r][c];                 // filled cell shown as its digit
                if (c < SIZE - 1) out << ' ';            // space between cells
            }                                            // end column loop
            out << '\n';                                 // end of the row
        }                                                // end row loop
    }                                                    // end print

private:                                                 // internal data
    int cells[SIZE][SIZE];                               // fixed 81-int grid, no heap use
};                                                       // end class SudokuBoard

// ---------------------------------------------------------------------------
// SudokuSolver: recursive DFS with backtracking that prints every solution.
// Bitmasks track which digits each row, column, and block already uses.
// Source: Claude baseline, revised.
// ---------------------------------------------------------------------------
class SudokuSolver {                                     // the recursive search
public:                                                  // members usable by PuzzleRunner
    explicit SudokuSolver(const SudokuBoard& start)      // constructor: copy puzzle, build masks
        : board(start), solutionCount(0), givensValid(true) {  // initialize members
        for (int i = 0; i < SudokuBoard::SIZE; ++i)      // for each row/column/block index
            rowUsed[i] = colUsed[i] = boxUsed[i] = 0;    // start with no digits used
        for (int r = 0; r < SudokuBoard::SIZE; ++r) {    // scan every row
            for (int c = 0; c < SudokuBoard::SIZE; ++c) {// scan every column
                int v = board.get(r, c);                 // digit in this cell
                if (v == SudokuBoard::EMPTY) continue;   // skip blank cells
                int bit = 1 << v;                        // bit v stands for digit v
                if ((rowUsed[r] | colUsed[c] | boxUsed[boxIndex(r, c)]) & bit)  // digit already seen?
                    givensValid = false;                 // givens conflict: unsolvable
                place(r, c, v);                          // record the given digit
            }                                            // end column loop
        }                                                // end row loop
    }                                                    // end constructor

    int solveAndPrint(std::ostream& out) {               // run search; return number of solutions
        solutionCount = 0;                               // reset the counter
        if (givensValid) search(out);                    // only search valid puzzles
        return solutionCount;                            // 0 means no solution
    }                                                    // end solveAndPrint

private:                                                 // internal state and helpers
    static const int ALL_DIGITS = 0x3FE;                 // bits 1-9 set (digits 1 to 9)

    SudokuBoard board;                                   // working copy filled during search
    int rowUsed[SudokuBoard::SIZE];                      // bit d set: digit d used in that row
    int colUsed[SudokuBoard::SIZE];                      // bit d set: digit d used in that column
    int boxUsed[SudokuBoard::SIZE];                      // bit d set: digit d used in that block
    int solutionCount;                                   // solutions printed so far
    bool givensValid;                                    // false if givens conflict

    static int boxIndex(int r, int c) {                  // block number 0-8 for a cell
        return (r / SudokuBoard::BOX) * SudokuBoard::BOX + c / SudokuBoard::BOX;  // row band * 3 + column band
    }                                                    // end boxIndex

    void place(int r, int c, int v) {                    // put digit v at (r, c)
        board.set(r, c, v);                              // fill the cell
        rowUsed[r] |= 1 << v;                            // mark digit in row
        colUsed[c] |= 1 << v;                            // mark digit in column
        boxUsed[boxIndex(r, c)] |= 1 << v;               // mark digit in block
    }                                                    // end place

    void remove(int r, int c, int v) {                   // undo place() (the backtrack)
        board.set(r, c, SudokuBoard::EMPTY);             // blank the cell again
        rowUsed[r] &= ~(1 << v);                         // unmark digit in row
        colUsed[c] &= ~(1 << v);                         // unmark digit in column
        boxUsed[boxIndex(r, c)] &= ~(1 << v);            // unmark digit in block
    }                                                    // end remove

    int candidates(int r, int c) const {                 // legal digits for (r, c) as a bitmask
        return ALL_DIGITS & ~(rowUsed[r] | colUsed[c] | boxUsed[boxIndex(r, c)]);  // digits not used nearby
    }                                                    // end candidates

    static int countBits(int mask) {                     // how many digits a mask holds
        int n = 0;                                       // running count
        while (mask) {                                   // until no bits remain
            mask &= mask - 1;                            // clear the lowest set bit
            ++n;                                         // count it
        }                                                // end loop
        return n;                                        // number of set bits
    }                                                    // end countBits

    bool chooseCell(int& bestR, int& bestC, int& bestMask) const {  // find most constrained empty cell
        bestR = -1; bestC = -1; bestMask = 0;            // default: grid is full
        int bestCount = SudokuBoard::SIZE + 1;           // larger than any real count
        for (int r = 0; r < SudokuBoard::SIZE; ++r) {    // scan every row
            for (int c = 0; c < SudokuBoard::SIZE; ++c) {// scan every column
                if (board.get(r, c) != SudokuBoard::EMPTY) continue;  // only blank cells
                int mask = candidates(r, c);             // legal digits for this cell
                int count = countBits(mask);             // how many there are
                if (count == 0) return false;            // dead end: caller backtracks
                if (count < bestCount) {                 // fewer options than best so far
                    bestR = r; bestC = c;                // remember this cell
                    bestMask = mask; bestCount = count;  // and its options
                    if (count == 1) return true;         // a forced cell cannot be beaten
                }                                        // end best check
            }                                            // end column loop
        }                                                // end row loop
        return true;                                     // best cell found (or grid full)
    }                                                    // end chooseCell

    void search(std::ostream& out) {                     // recursive depth-first search
        int bestR, bestC, bestMask;                      // cell to fill and its options
        if (!chooseCell(bestR, bestC, bestMask)) return; // impossible branch: back up
        if (bestR == -1) {                               // no blanks left: a solution
            ++solutionCount;                             // count it
            out << "Solution " << solutionCount << ":\n";// label it
            board.print(out);                            // print it right away
            out << '\n';                                 // blank line after it
            return;                                      // keep searching for more
        }                                                // end solution case
        for (int v = 1; v <= SudokuBoard::SIZE; ++v) {   // try digits 1 to 9
            if (!(bestMask & (1 << v))) continue;        // skip digits that are not legal
            place(bestR, bestC, v);                      // tentatively fill the cell
            search(out);                                 // recurse on the smaller puzzle
            remove(bestR, bestC, v);                     // undo and try the next digit
        }                                                // end digit loop
    }                                                    // end search
};                                                       // end class SudokuSolver

// ---------------------------------------------------------------------------
// PuzzleRunner: handles one file end to end (load, print, solve, report).
// Source: Claude baseline, revised.
// ---------------------------------------------------------------------------
class PuzzleRunner {                                     // processes a single puzzle file
public:                                                  // members usable by SudokuApp
    explicit PuzzleRunner(const std::string& file) : fileName(file) {}  // remember the file name

    void run(std::ostream& out) const {                  // load, print, solve, report
        out << "==============================\n";      // header line
        out << "File: " << fileName << "\n";             // required: file name
        out << "==============================\n";      // header line

        SudokuBoard puzzle;                              // grid to load into
        std::string error;                               // reason if loading fails
        if (!puzzle.loadFromFile(fileName, error)) {     // load and validate
            out << "Error reading " << fileName << ": " << error << "\n\n";  // report the problem
            return;                                      // skip to the next file
        }                                                // end load check

        out << "Puzzle:\n";                              // label the original puzzle
        puzzle.print(out);                               // required: puzzle as read
        out << '\n';                                     // blank line after it

        SudokuSolver solver(puzzle);                     // solver for this puzzle
        int found = solver.solveAndPrint(out);           // required: print every solution
        if (found == 0) out << "No solution found\n";    // required message when none exist
        out << "Solutions found: " << found << "\n\n";   // total for this file
    }                                                    // end run

private:                                                 // internal data
    std::string fileName;                                // which puzzle file to process
};                                                       // end class PuzzleRunner

// ---------------------------------------------------------------------------
// SudokuApp: runs the five puzzle files in order. Source: Claude baseline.
// ---------------------------------------------------------------------------
class SudokuApp {                                        // top-level application object
public:                                                  // members usable by main
    SudokuApp() {                                        // constructor: build the file list
        for (int i = 1; i <= 5; ++i)                     // puzzle numbers 1 to 5
            files.push_back("puzzle" + std::to_string(i) + ".txt");  // e.g. "puzzle1.txt"
    }                                                    // end constructor

    void run(std::ostream& out) const {                  // process every file
        for (std::size_t i = 0; i < files.size(); ++i) { // each file in order
            PuzzleRunner runner(files[i]);               // one runner per file
            runner.run(out);                             // print everything for it
        }                                                // end file loop
    }                                                    // end run

private:                                                 // internal data
    std::vector<std::string> files;                      // the five puzzle file names
};                                                       // end class SudokuApp

int main() {                                             // program entry point
    SudokuApp app;                                       // create the application object
    app.run(std::cout);                                  // solve and print all five puzzles
    return 0;                                            // normal exit
}                                                        // end main
