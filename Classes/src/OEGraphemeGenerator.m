//  OpenEars 
//  http://www.politepix.com/openears
//
//  OEGraphemeGenerator.m
//  OpenEars
// 
//  OEGraphemeGenerator is a class which creates pronunciations for words which aren't in the dictionary
//
//  Copyright Politepix UG (haftungsbeschränkt) 2014. All rights reserved.
//  http://www.politepix.com
//  Contact at http://www.politepix.com/contact
//
//  this file is licensed under the Politepix Shared Source license found 
//  found in the root of the source distribution. Please see the file "Version.txt" in the root of 
//  the source distribution for the version number of this OpenEars package.


#import "OEGraphemeGenerator.h"
#import "flite.h"

#import "OERuntimeVerbosity.h"
#import <sphinxbase/ngram_model.h>
#import <string.h>
#import <sphinxbase/err.h>
#import <sphinxbase/glist.h>
#import <sphinxbase/ckd_alloc.h>
#import <sphinxbase/strfuncs.h>
#import <limits.h>

extern int openears_logging;

cst_voice *graphemeGenerationVoice;
@implementation OEGraphemeGenerator

void unregister_cmu_us_kal_phon(cst_voice *vox);
cst_voice *register_cmu_us_kal_phon(const char *voxdir);

- (instancetype) init {
    self = [super init];
    if (self) {
        flite_init();
        NSMutableCharacterSet *acceptableCharacters = [NSMutableCharacterSet alphanumericCharacterSet];
        [acceptableCharacters addCharactersInString:@"'-"];
        _charactersToRemove = [acceptableCharacters invertedSet];
    }
    return self;
}

- (NSString *) convertGraphemes:(NSString *)word {
    NSTimeInterval start = [NSDate timeIntervalSinceReferenceDate];
    cst_features *extra_feats = new_features();
    feat_set_string(extra_feats, "print_info_relation", "Segment");
    graphemeGenerationVoice = register_cmu_us_kal_phon(NULL);
    feat_copy_into(extra_feats, graphemeGenerationVoice->features);
    delete_features(extra_feats);
    
    if (openears_logging == 1) NSLog(@"Using convertGraphemes for the word or phrase %@ which doesn't appear in the dictionary", word);
    
    cst_utterance *utterance = flite_synth_text((char *)[word UTF8String], graphemeGenerationVoice);
    
    NSMutableString *phonesMutableString = [[NSMutableString alloc] init];
    
    cst_item *item;
    
    const char *relname = utt_feat_string(utterance,"print_info_relation");
    
    for (item = relation_head(utt_relation(utterance, relname)); item; item = item_next(item)) {
        
        NSString *bufferString = [NSString stringWithFormat:@"%s", item_feat_string(item, "name")];
        
        [phonesMutableString appendString:[NSString stringWithFormat:@"%@ ",bufferString]];
    }
    
    const char *destinationString = [(NSString *)phonesMutableString UTF8String];
    
    delete_utterance(utterance);
    
    NSString *stringToReturn = [NSString stringWithFormat:@"%s", destinationString];
    unregister_cmu_us_kal_phon(graphemeGenerationVoice);
    if (openears_logging == 1) NSLog(@"Elapsed time to generate unknown word phonemes in English is %f", [NSDate timeIntervalSinceReferenceDate] - start);
    return stringToReturn;
}

- (NSString *) topHitFromProbabilitySortedArray:(NSMutableArray *)array verbose:(BOOL)verbose {
    if (!array || [array count] == 0 || !array[0] || [array[0] count] == 0) {
        return nil;
    }
    return array[0][0];
}

- (NSArray *) topHitFromProbabilitySortedArray:(NSMutableArray *)array startingWithPhoneme:(NSString *)phoneme forWord:(NSString *)word verbose:(BOOL)verbose {
    if (!array) {
        return nil;
    }
    
    if (!phoneme) {
        if (verbose)NSLog(@"There was a match for %@ and there was no last phoneme so we'll just return the top hit: %@",word,array[0][0]);
        return array[0];
    } else {
        
        if (verbose) NSLog(@"There was a last phoneme of %@ so we're trying to process the array %@ to find it in a high hit.", phoneme, array);
        for (NSMutableArray *probArray in array) {
            if ([[[probArray[0] componentsSeparatedByString:@" "]firstObject] isEqualToString:phoneme]) {
                if (verbose) NSLog(@"probArray[0] (%@) starts with the right phoneme so returning it.", probArray[0]);
                return probArray;   
            } 
        }
    }
    if (verbose) NSLog(@"There were no correct matches so returning nil");
    return nil;
}

- (NSString *) checkForEarlyExitForWord:(NSString *)word inDictionary:(NSDictionary *)lookupDictionary verbose:(BOOL)verbose {
    
    NSString *largestPossibleFirstPhoneme = nil;
    
    [self topHitFromProbabilitySortedArray:[[lookupDictionary objectForKey:[word substringToIndex:1]] objectForKey:word] verbose:verbose];
    
    if (!largestPossibleFirstPhoneme) { // First see if this whole word is obtainable in a single entry and quickly return if so.
        NSString *noBracketsWord = [word stringByReplacingOccurrencesOfString:@">" withString:@""];
        largestPossibleFirstPhoneme = [self topHitFromProbabilitySortedArray:[[lookupDictionary objectForKey:[noBracketsWord substringToIndex:1]] objectForKey:noBracketsWord] verbose:verbose];
        if (!largestPossibleFirstPhoneme) {
            noBracketsWord = [[word stringByReplacingOccurrencesOfString:@"<" withString:@""]stringByReplacingOccurrencesOfString:@">" withString:@""];
            largestPossibleFirstPhoneme = [self topHitFromProbabilitySortedArray:[[lookupDictionary objectForKey:[noBracketsWord substringToIndex:1]] objectForKey:noBracketsWord] verbose:verbose];
            if (!largestPossibleFirstPhoneme) {
                noBracketsWord = [word stringByReplacingOccurrencesOfString:@"<" withString:@""];
                largestPossibleFirstPhoneme = [self topHitFromProbabilitySortedArray:[[lookupDictionary objectForKey:[noBracketsWord substringToIndex:1]] objectForKey:noBracketsWord] verbose:verbose];
            }
        } 
    }
    
    return largestPossibleFirstPhoneme;
}

- (NSString *) examineSubstringsInTopHitArray:(NSMutableArray *)topHitArray usingSubstring:(NSString *)substring lastPhoneme:(NSString *)lastPhoneme letterDictionary:(NSDictionary *)letterDictionary withSubstringBoundary:(NSInteger *)substringBoundary verbose:(BOOL)verbose { // Check for whether the top hit was the most probable possible hit or whether there are shorter segments that are more probable
    
    NSMutableArray *topHitMutableArray = [[NSMutableArray alloc] init];
    
    if (verbose) NSLog(@"Hit for substring %@, now checking if there are higher prob hits for shorter segments.", substring);
    
    [topHitMutableArray addObject:@[topHitArray[0], topHitArray[1], substring]];
    
    for (int i = 2; i < 4; i++) { 
        
        if ((!lastPhoneme && (([substring length] - i > 0) && ([substring length] - i < 200))) || (lastPhoneme && (([substring length] - i > 1) && ([substring length] - i < 200)))) {
            NSString *shortenedSubstring = [substring substringToIndex:[substring length] - i];
            NSArray *possibleResultsArray = [self topHitFromProbabilitySortedArray:[letterDictionary objectForKey:shortenedSubstring] startingWithPhoneme:lastPhoneme forWord:substring verbose:verbose];
            if (possibleResultsArray && ![shortenedSubstring isEqualToString:@"<"]) {
                [topHitMutableArray addObject:@[possibleResultsArray[0], possibleResultsArray[1], shortenedSubstring]];
            }
        }
    }
    
    [topHitMutableArray sortUsingComparator:^NSComparisonResult(id a, id b) {
        NSNumber *number1 = [a objectAtIndex:1];
        NSNumber *number2 = [b objectAtIndex:1];
        return [number2 compare:number1];
    }];
    
    if (topHitMutableArray && [topHitMutableArray count] > 0 && topHitMutableArray[0] && [topHitMutableArray[0] count] > 0 && topHitMutableArray[0][0] && [topHitMutableArray[0][0] length] > 0 && topHitMutableArray[0][2]) {
        
        NSString *possibleTopHit = topHitMutableArray[0][0];
        NSString *possibleTopSubstring = topHitMutableArray[0][2];
        
        
        if (possibleTopHit && possibleTopSubstring && [possibleTopHit length] > 0 && [possibleTopSubstring length] > 0) {
            NSInteger offset = [substring length] - [possibleTopSubstring length];
            substring = possibleTopSubstring;
            *substringBoundary = *substringBoundary - offset;
            if (verbose) {
                if (offset != 0) {
                    NSLog(@"There was a higher probability for a shorter segment: winning tophit was %@ and substring is now %@, this is %@ shorter than the original substring.", possibleTopHit, substring, @(offset));
                    return possibleTopHit;
                } else {
                    NSLog(@"No, the top probability was for the original hit of %@ so substring is not going to be prematurely shortened.", possibleTopHit);                        
                }
            }
            
        }
    }
    return nil;
}

- (NSString *) stripPounds:(NSString *)phonemes {
    return   [[[[[phonemes stringByReplacingOccurrencesOfString:@"#" withString:@" "]stringByReplacingOccurrencesOfString:@"     " withString:@" "]stringByReplacingOccurrencesOfString:@"    " withString:@" "]stringByReplacingOccurrencesOfString:@"   " withString:@" "]stringByReplacingOccurrencesOfString:@"  " withString:@" "];
}

- (NSString *) getPhonemesForWordUsingGeneralizedG2P:(NSString *)word usingG2PDictionary:(NSDictionary *)lookupDictionary forAcousticModelNamed:(NSString *)acousticModelName usingUnigramsOnly:(BOOL)unigramsOnly verbose:(BOOL)verbose {
    
    word = [[[word componentsSeparatedByCharactersInSet:self.charactersToRemove] componentsJoinedByString:@""] lowercaseString]; // Get rid of characters we don't do g2p on
        
    NSString *phonemesToReturn = nil;
    
    if (([acousticModelName rangeOfString:@"AcousticModelEnglish"].location != NSNotFound) || ([acousticModelName rangeOfString:@"AcousticModelAlternateEnglish"].location != NSNotFound)) {
        if (verbose)NSLog(@"This is an English model so making an exception to the generalized g2p fallback method.");
        phonemesToReturn = [[[[self convertGraphemes:word] stringByReplacingOccurrencesOfString:@"ax" withString:@"ah"]stringByReplacingOccurrencesOfString:@"pau " withString:@""]uppercaseString];
   
    } else {
        
        // From start to finish, grab the largest possible chunk and always make sure that the next chunk starts with the last letter AND last phoneme of the previous chunk if possible, otherwise starting a new large-as-possible chunk using neither. i.e. the end of the last chunk is always our anchor for our swing to the next one if that is possible.
        if (verbose)NSLog(@"\n\n");
        if ([word length] < 1 || [word isEqualToString:@" "]) return nil;
        if (!unigramsOnly)word = [NSString stringWithFormat:@"<%@>", word]; // if this isn't in a unigrams-only model, add some start/end tokens
        if (verbose) NSLog(@"Processing %@", word);
        NSMutableString *foundPhonemes = [[NSMutableString alloc] init]; // Get ready to store some phonemes
        
        NSString *largestPossibleFirstPhoneme = [self checkForEarlyExitForWord:word inDictionary:lookupDictionary verbose:verbose];
        
        if (largestPossibleFirstPhoneme) {
            if (verbose)NSLog(@"found largestPossibleFirstPhoneme for %@ so not using exiting early", word);
            return largestPossibleFirstPhoneme; // If the whole word can be found in one chunk, return it immediately, otherwise:
        }
        
        NSString *lastPhoneme = nil; // Store the previous last phoneme to maintain the chain if possible
        NSMutableString *mutableWord = [[NSMutableString alloc] initWithString:word]; // Convert into a mutable form
        
        while ([mutableWord length] > 0) { // While we have grapheme clusters to attempt to match,
            if (verbose)NSLog(@"G2P for %@: still have letters in mutableWord %@ so continuing", word,mutableWord);
            NSDictionary *letterDictionary = [lookupDictionary objectForKey:[mutableWord substringToIndex:1]]; // Get the relevant letter-indexed dictionary for fast lookups
            for (NSInteger substringBoundary = [mutableWord length]; substringBoundary > 0; substringBoundary--) { // Trimming the word from the end towards the start,
                
                NSString *substring = [mutableWord substringToIndex:substringBoundary]; // get the largest possible match for the trimmed substring.
                
                if (verbose)NSLog(@"G2P for %@: substring is %@, using letterDictionary for letter %@", word,substring,[substring substringToIndex:1]);
                
                NSString *topHit = nil;
                
                NSArray *topHitArray = [self topHitFromProbabilitySortedArray:[letterDictionary objectForKey:substring] startingWithPhoneme:lastPhoneme forWord:substring verbose:verbose];
                
                if (topHitArray) {
                    topHit = topHitArray[0];
                }
                
                if (topHit) { // If we get a match and either a) there was no previous last phoneme to worry about, or b) the first phoneme of the match matches the last phoneme of the previous match,
                    lastPhoneme = [[topHit componentsSeparatedByString:@" "]lastObject]; // Remember the last phoneme,
                    if (verbose)NSLog(@"G2P for %@: found a match for substring %@ (%@), saving %@ as last phoneme", word,substring, topHit,lastPhoneme);
                    NSInteger deleteToLength = substringBoundary - 1; // Figure out if we're deleting everything _but_ the last character in the found substring or
                    
                    if ([substring length]==[mutableWord length] || [[mutableWord substringFromIndex:[substring length]] isEqualToString:@">"]) {
                        deleteToLength = [mutableWord length]; // If this is the last chunk just deleting everything
                        if (verbose)NSLog(@"G2P for %@: %@ is the last chunk so deleting everything", word,substring);
                    } else { 
                        if (verbose)NSLog(@"G2P for %@: %@ is not the last chunk so deleting up but not including to last letter found", word,substring);
                    }
                    [mutableWord deleteCharactersInRange:NSMakeRange(0, deleteToLength)]; // And removing what we found but the last character, or everything we found if that was the last chunk.
                    
                    [foundPhonemes appendString:topHit];
                    [foundPhonemes appendString:@" "];
                    if (verbose)NSLog(@"G2P for %@: appending %@ to foundPhonemes so it now reads %@", word,topHit, foundPhonemes);
                    break;
                } else {
                    if (
                       ([substring rangeOfString:@">"].location != NSNotFound && ((lastPhoneme && [substring length]==3) || ([substring length]==2))) 
                       || 
                       ((lastPhoneme && [substring length]==2) || ([substring length]==1))
                       ) { // If we've gotten down to one new character with no acceptable matches and there is a last phoneme, let go of the monkeybar and stop worrying about the previous last character and last phoneme and start from the biggest possible remaining chunk again. Do the same if we're down to a single character and it has no matches.
                        if (verbose)NSLog(@"G2P for %@: substring length is %@ and lastPhoneme is %@ and there is no match so we're letting go of the shared phoneme", word,@([substring length]),lastPhoneme);
                        [mutableWord deleteCharactersInRange:NSMakeRange(0, 1)];
                        if (verbose)NSLog(@"G2P for %@: deleting first letter of mutableWord which is now %@", word,mutableWord);
                        lastPhoneme = nil;
                        break;
                    } else {
                        if (verbose)NSLog(@"G2P for %@: no match at all for substring %@", word,substring);
                        
                    }
                }
            } 
        }
        
        phonemesToReturn = [self stripPounds:foundPhonemes];
        phonemesToReturn = [phonemesToReturn stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        
        NSArray *phonemesPotentialDoublesArray = [phonemesToReturn componentsSeparatedByString:@" "];
        NSMutableArray *phonemesNoDoublesArray = [[NSMutableArray alloc] init];
        
        for (int index = 0; index < [phonemesPotentialDoublesArray count]; index++) {
            
            NSString *phoneme = [phonemesPotentialDoublesArray objectAtIndex:index];
            
            if ((index > 0 && ![[phonemesPotentialDoublesArray objectAtIndex:(index - 1)] isEqualToString:phoneme]) || index == 0) { // If this isn't the first phoneme we can look back and see if it's equal to the preceding one, or just add it if it's the first phoneme
                [phonemesNoDoublesArray addObject:phoneme];
            }
        }
        phonemesToReturn = [phonemesNoDoublesArray componentsJoinedByString:@" "];
    }

    return phonemesToReturn; // Return it.
}

@end
