#!/usr/bin/env python

import random # for seed, random
import sys    # for stdout

def generateCacheKeyFromStrands(strand1, strand2):
	return strand1 + '$' + strand2

def generateCacheKey(res):
	return generateCacheKeyFromStrands(res['strand1'], res['strand2'] )

def findOptimalAlignmentCached(strand1, strand2, cache):
	cacheKey = generateCacheKeyFromStrands(strand1, strand2)
	if cacheKey in cache: return cache[cacheKey]
	res = findOptimalAlignment(strand1, strand2, cache)
	cache[cacheKey] = res
	return res

# Computes the score of the optimal alignment of two DNA strands.
def findOptimalAlignment(strand1, strand2, cache):
	
	# if one of the two strands is empty, then there is only
	# one possible alignment, and of course it's optimal.
	# -- who needs cache for base case?
	if len(strand1) == 0: 
		return {
			'score': len(strand2) * -2,
			'strand1': ' ' * len(strand2),
			'strand2': strand2
		}
	if len(strand2) == 0: 
		return {
			'score': len(strand1) * -2,
			'strand1': strand1,
			'strand2': ' ' * len(strand1)
		}

	# There's the scenario where the two leading bases of
	# each strand are forced to align, regardless of whether or not
	# they actually match.
	bestWith = findOptimalAlignmentCached(strand1[1:], strand2[1:], cache) 
	if strand1[0] == strand2[0]: 
		res = {
			'score': bestWith['score'] + 1, # no benefit from making other recursive calls
			'strand1': strand1[0] + bestWith['strand1'],
			'strand2': strand1[0] + bestWith['strand2']
		}
		return res

	bestWith['score'] -= 1
	bestWith['strand1'] = strand1[0] + bestWith['strand1']
	bestWith['strand2'] = strand2[0] + bestWith['strand2']
	best = bestWith
	
	# It's possible that the leading base of strand1 best
	# matches not the leading base of strand2, but the one after it.
	bestWithout = findOptimalAlignmentCached(strand1, strand2[1:], cache)
	bestWithout['score'] -= 2 # penalize for insertion of space
	bestWithout['strand1'] = ' ' + bestWithout['strand1']
	bestWithout['strand2'] = strand2[0] + bestWithout['strand2']
	if bestWithout['score'] > best['score']:
		best = bestWithout

	# opposite scenario
	bestWithout = findOptimalAlignmentCached(strand1[1:], strand2, cache)
	bestWithout['score'] -= 2 # penalize for insertion of space	
	bestWithout['strand1'] = strand1[0] + bestWithout['strand1']
	bestWithout['strand2'] = ' ' + bestWithout['strand2']
	if bestWithout['score'] > best['score']:
		best = bestWithout

	return best

# Utility function that generates a random DNA string of
# a random length drawn from the range [minlength, maxlength]
def generateRandomDNAStrand(minlength, maxlength):
	assert minlength > 0, \
	       "Minimum length passed to generateRandomDNAStrand" \
	       "must be a positive number" # these \'s allow mult-line statements
	assert maxlength >= minlength, \
	       "Maximum length passed to generateRandomDNAStrand must be at " \
	       "as large as the specified minimum length"
	strand = ""
	length = random.choice(xrange(minlength, maxlength + 1))
	bases = ['A', 'T', 'G', 'C']
	for i in xrange(0, length):
		strand += random.choice(bases)
	return strand

# Method that just prints out the supplied alignment score.
# This is more of a placeholder for what will ultimately
# print out not only the score but the alignment as well.

def printAlignment(score, out = sys.stdout):	
	out.write("Optimal alignment score is " + str(score['score']) + "\n")
	out.write('\n')
	out.write('  +  ')
	out.write(
		''.join(map(lambda x: '1' if x[0] == x[1] and x[0] != ' ' and x[1] != ' ' else ' ', zip(score['strand1'], score['strand2']) ) )
	)
	out.write('\n')
	out.write(' ' * 5)
	out.write(str(score['strand1']) + "\n")
	out.write(' ' * 5)
	out.write(str(score['strand2']) + "\n")
	out.write('  -  ')
	out.write(
		''.join(map(lambda x: '2' if x[0] == ' ' or x[1] == ' ' else '1' if x[0] != x[1] else ' ', zip(score['strand1'], score['strand2']) ) )
	)
	out.write('\n\n')

# Unit test main in place to do little more than
# exercise the above algorithm.  As written, it
# generates two fairly short DNA strands and
# determines the optimal alignment score.
#
# As you change the implementation of findOptimalAlignment
# to use memoization, you should change the 8s to 40s and
# the 10s to 60s and still see everything execute very
# quickly.
 
def main():
	while (True):
		sys.stdout.write("Generate random DNA strands? ")
		answer = sys.stdin.readline()
		if answer == "no\n": break
		strand1 = generateRandomDNAStrand(50, 70)
		strand2 = generateRandomDNAStrand(50, 70)
		sys.stdout.write("Aligning these two strands: " + strand1 + "\n")
		sys.stdout.write("                            " + strand2 + "\n")
		cache = {}
		alignment = findOptimalAlignment(strand1, strand2, cache)
		printAlignment(alignment)
		
if __name__ == "__main__":
  main()
