/*
 * Program name : EECS 348 Assignment 4 - Recursive Object-Oriented Sudoku Solver
 * Description  : Reads five Sudoku puzzles from text files and finds every
 *                solution to each one with a recursive depth-first search with
 *                backtracking. Each empty cell is filled by trying every digit
 *                that does not conflict with its row, column, or 3x3 block.
 * Inputs       : puzzle1.txt, puzzle2.txt, puzzle3.txt, puzzle4.txt, puzzle5.txt
 *                (9 lines of 9 space-separated entries; digits 1-9 are givens,
 *                an underscore "_" is a blank cell), read from the current
 *                working directory.
 * Output       : For each file: the file name, the puzzle as read from the file,
 *                every solution found (or "No solution found"), and the number
 *                of solutions found.
 * Collaborators: None (no other people).
 * Other sources: Claude Opus 5.5 (Anthropic, claude.ai) generated the baseline
 *                program (class structure, file loading, recursive search,
 *                driver). Mistral Small 4 (Mistral Le Chat) generated the
 *                alternative that was compared against it. See
 *                Assignment4GenAI.pdf for the prompt, both raw programs, and
 *                the analysis. The bitmask bookkeeping, most-constrained-cell
 *                selection, and printing solutions as they are found are my
 *                modifications.
 * Author       : Nicolas Villalobos
 * Created      : October 15, 2026
 * Revised      : October 15, 2026
 */

#include <fstream>   // std::ifstream for reading the puzzle files
#include <iostream>  // std::cout for printing results
#include <string>    // std::string for file names and tokens
#include <vector>    // std::vector for the list of puzzle file names

// ---------------------------------------------------------------------------
// SudokuBoard: stores one 9x9 grid (0 = blank), loads it from a file, and
// prints it. Source: Claude baseline. My changes: removed the rule-checking
// methods (that job moved into SudokuSolver's bitmasks) and print the grid in
// the same layout as the input file.
// ---------------------------------------------------------------------------
class SudokuBoard {
public:
    static const int SIZE = 9;                   // rows / columns in the grid
    static const int BOX = 3;                    // side length of one 3x3 block
    static const int EMPTY = 0;                  // value stored in a blank cell

    SudokuBoard() {                              // constructor: start with an all-blank grid
        for (int r = 0; r < SIZE; ++r)           // every row
            for (int c = 0; c < SIZE; ++c)       // every column in that row
                cells[r][c] = EMPTY;             // mark the cell blank
    }

    // Reads 81 entries from the file. Returns false and fills "error" if the
    // file is missing, too short, or holds anything other than 1-9 or "_".
    bool loadFromFile(const std::string& fileName, std::string& error) {
        std::ifstream in(fileName.c_str());      // open the puzzle file
        if (!in) {                               // file missing or unreadable
            error = "Could not open file";       // say why
            return false;                        // report failure
        }
        for (int r = 0; r < SIZE; ++r) {         // read row by row
            for (int c = 0; c < SIZE; ++c) {     // each entry in the row
                std::string token;               // one space-separated entry
                if (!(in >> token)) {            // ran out of entries early
                    error = "File ended early (expected 81 entries)";
                    return false;                // cannot build a full grid
                }
                if (token == "_") {              // underscore = blank cell
                    cells[r][c] = EMPTY;         // store it as 0
                } else if (token.size() == 1 && token[0] >= '1' && token[0] <= '9') {
                    cells[r][c] = token[0] - '0';// digit character -> integer
                } else {                         // anything else is invalid
                    error = "Invalid entry '" + token + "' at row " +
                            std::to_string(r + 1) + ", column " + std::to_string(c + 1);
                    return false;                // reject the malformed file
                }
            }
        }
        return true;                             // all 81 entries read successfully
    }

    int get(int row, int col) const { return cells[row][col]; }       // read one cell
    void set(int row, int col, int value) { cells[row][col] = value; } // write one cell

    // Prints the grid in the input file's format: digits, "_" for blanks.
    void print(std::ostream& out) const {
        for (int r = 0; r < SIZE; ++r) {         // one line per row
            for (int c = 0; c < SIZE; ++c) {     // each cell in the row
                if (cells[r][c] == EMPTY) out << '_';  // blank cell
                else out << cells[r][c];               // filled cell
                if (c < SIZE - 1) out << ' ';          // space between cells
            }
            out << '\n';                         // end of the row
        }
    }

private:
    int cells[SIZE][SIZE];                       // fixed 81-int storage, no heap use
};

// ---------------------------------------------------------------------------
// SudokuSolver: recursive DFS with backtracking that prints every solution.
// Source: recursion shape from Claude baseline (pick an empty cell, try each
// legal digit, recurse, undo). My changes: row/column/block bitmasks for O(1)
// legality checks, picking the empty cell with the fewest candidates,
// detecting dead ends before recursing, a single-pass givens check, and
// printing each solution immediately instead of storing all of them.
// ---------------------------------------------------------------------------
class SudokuSolver {
public:
    // Copies the puzzle, builds the "digit used" masks, and checks the givens.
    explicit SudokuSolver(const SudokuBoard& start)
        : board(start), solutionCount(0), givensValid(true) {
        for (int i = 0; i < SudokuBoard::SIZE; ++i)      // clear all 27 masks
            rowUsed[i] = colUsed[i] = boxUsed[i] = 0;
        for (int r = 0; r < SudokuBoard::SIZE; ++r) {    // scan every cell once
            for (int c = 0; c < SudokuBoard::SIZE; ++c) {
                int v = board.get(r, c);                 // digit in this cell
                if (v == SudokuBoard::EMPTY) continue;   // blanks have nothing to record
                int bit = 1 << v;                        // bit v stands for digit v
                if ((rowUsed[r] | colUsed[c] | boxUsed[boxIndex(r, c)]) & bit)
                    givensValid = false;                 // digit already seen: givens conflict
                place(r, c, v);                          // record the given digit
            }
        }
    }

    // Runs the search, printing each solution as it is found.
    // Returns how many solutions were found (0 means none exist).
    int solveAndPrint(std::ostream& out) {
        solutionCount = 0;                               // reset the counter
        if (givensValid) search(out);                    // conflicting givens can never be solved
        return solutionCount;                            // caller decides what to print for 0
    }

private:
    static const int ALL_DIGITS = 0x3FE;                 // bits 1-9 set (digits 1 to 9)

    SudokuBoard board;                                   // working copy filled in during search
    int rowUsed[SudokuBoard::SIZE];                      // bit d set: digit d is in that row
    int colUsed[SudokuBoard::SIZE];                      // bit d set: digit d is in that column
    int boxUsed[SudokuBoard::SIZE];                      // bit d set: digit d is in that block
    int solutionCount;                                   // solutions printed so far
    bool givensValid;                                    // false if the file's givens conflict

    // Block number 0-8 for a cell (left to right, top to bottom).
    static int boxIndex(int r, int c) {
        return (r / SudokuBoard::BOX) * SudokuBoard::BOX + c / SudokuBoard::BOX;
    }

    // Puts digit v in (r, c) and marks it used in that row, column, block.
    void place(int r, int c, int v) {
        board.set(r, c, v);                              // fill the cell
        rowUsed[r] |= 1 << v;                            // mark in row mask
        colUsed[c] |= 1 << v;                            // mark in column mask
        boxUsed[boxIndex(r, c)] |= 1 << v;               // mark in block mask
    }

    // Undoes place(): blanks the cell and unmarks the digit (the backtrack).
    void remove(int r, int c, int v) {
        board.set(r, c, SudokuBoard::EMPTY);             // empty the cell again
        rowUsed[r] &= ~(1 << v);                         // unmark in row mask
        colUsed[c] &= ~(1 << v);                         // unmark in column mask
        boxUsed[boxIndex(r, c)] &= ~(1 << v);            // unmark in block mask
    }

    // Legal digits for (r, c) as a bitmask: digits used in none of its
    // row, column, or block.
    int candidates(int r, int c) const {
        return ALL_DIGITS & ~(rowUsed[r] | colUsed[c] | boxUsed[boxIndex(r, c)]);
    }

    // Number of set bits in a mask (how many candidates a cell has).
    static int countBits(int mask) {
        int n = 0;                                       // running count
        while (mask) {                                   // until no bits are left
            mask &= mask - 1;                            // clear the lowest set bit
            ++n;                                         // count it
        }
        return n;
    }

    // Finds the empty cell with the fewest candidates. Sets bestR to -1 if
    // the grid is full. Returns false if some empty cell has no candidates,
    // meaning this branch can never lead to a solution.
    bool chooseCell(int& bestR, int& bestC, int& bestMask) const {
        bestR = -1; bestC = -1; bestMask = 0;            // default: no empty cell found
        int bestCount = SudokuBoard::SIZE + 1;           // larger than any real count
        for (int r = 0; r < SudokuBoard::SIZE; ++r) {    // scan every row
            for (int c = 0; c < SudokuBoard::SIZE; ++c) {// and every column
                if (board.get(r, c) != SudokuBoard::EMPTY) continue;  // only blank cells
                int mask = candidates(r, c);             // legal digits for this cell
                int count = countBits(mask);             // how many there are
                if (count == 0) return false;            // dead end: caller backtracks
                if (count < bestCount) {                 // more constrained than the best so far
                    bestR = r; bestC = c;                // remember this cell
                    bestMask = mask; bestCount = count;  // and its candidates
                    if (count == 1) return true;         // a forced cell cannot be beaten
                }
            }
        }
        return true;                                     // best cell (or full grid) found
    }

    // Recursive depth-first search with backtracking.
    void search(std::ostream& out) {
        int bestR, bestC, bestMask;                      // cell to fill and its candidates
        if (!chooseCell(bestR, bestC, bestMask)) return; // impossible branch: back up
        if (bestR == -1) {                               // base case: grid is full
            ++solutionCount;                             // count this solution
            out << "Solution " << solutionCount << ":\n";
            board.print(out);                            // print it right away
            out << '\n';
            return;                                      // keep searching for more
        }
        for (int v = 1; v <= SudokuBoard::SIZE; ++v) {   // try each digit in order
            if (!(bestMask & (1 << v))) continue;        // skip digits that are not legal here
            place(bestR, bestC, v);                      // tentatively fill the cell
            search(out);                                 // solve the rest of the grid (recursion)
            remove(bestR, bestC, v);                     // undo, then try the next digit
        }
    }
};

// ---------------------------------------------------------------------------
// PuzzleRunner: handles one file end to end (load, print, solve, report).
// Source: Claude baseline. My changes: uses the streaming solver and ends
// with a solution count line in the same style as solution1.txt.
// ---------------------------------------------------------------------------
class PuzzleRunner {
public:
    explicit PuzzleRunner(const std::string& file) : fileName(file) {}  // remember the file

    void run(std::ostream& out) const {
        out << "==============================\n";
        out << "File: " << fileName << "\n";             // required: puzzle file name
        out << "==============================\n";

        SudokuBoard puzzle;                              // grid to load into
        std::string error;                               // reason if loading fails
        if (!puzzle.loadFromFile(fileName, error)) {     // load and validate the file
            out << "Error reading " << fileName << ": " << error << "\n\n";
            return;                                      // skip to the next file
        }

        out << "Puzzle:\n";
        puzzle.print(out);                               // required: puzzle as read
        out << '\n';

        SudokuSolver solver(puzzle);                     // solver for this puzzle
        int found = solver.solveAndPrint(out);           // required: every solution
        if (found == 0) out << "No solution found\n";    // required message when none exist
        out << "Solutions found: " << found << "\n\n";   // total for this file
    }

private:
    std::string fileName;                                // which puzzle file to process
};

// ---------------------------------------------------------------------------
// SudokuApp: runs the five puzzle files in order. Source: Claude baseline.
// ---------------------------------------------------------------------------
class SudokuApp {
public:
    SudokuApp() {                                        // build the list of file names
        for (int i = 1; i <= 5; ++i)                     // puzzle1.txt to puzzle5.txt
            files.push_back("puzzle" + std::to_string(i) + ".txt");
    }

    void run(std::ostream& out) const {
        for (std::size_t i = 0; i < files.size(); ++i) { // each file in order
            PuzzleRunner runner(files[i]);               // one runner per file
            runner.run(out);                             // print everything for it
        }
    }

private:
    std::vector<std::string> files;                      // the five puzzle file names
};

int main() {                                             // entry point
    SudokuApp app;                                       // create the application object
    app.run(std::cout);                                  // solve and print all five puzzles
    return 0;                                            // normal exit
}
