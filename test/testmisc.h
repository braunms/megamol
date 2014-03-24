/*
 * testmisc.h
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 */

#ifndef VISLIBTEST_TESTMISC_H_INCLUDED
#define VISLIBTEST_TESTMISC_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

/*
 * Test functions for less automated tests of smaller vislib functions 
 */

void TestConsoleColours(void);

void TestColumnFormatter(void);

void TestTrace(void);

void TestExceptions(void);

void Test_system_message(void);

void TestPerformanceCounter(void);

void TestPathManipulations(void);

void TestFileNameSequence(void);

void TestAsciiFile(void);

void TestNamedColours(void);

#endif /* VISLIBTEST_TESTMISC_H_INCLUDED */
