//  OpenEars 
//  http://www.politepix.com/openears
//
//  OELanguageModelGenerator.m
//  OpenEars
//
//  OELanguageModelGenerator is a class which creates new grammars
//
//  Copyright Politepix UG (haftungsbeschränkt) 2014. All rights reserved.
//  http://www.politepix.com
//  Contact at http://www.politepix.com/contact
//
//  this file is licensed under the Politepix Shared Source license found 
//  found in the root of the source distribution. Please see the file "Version.txt" in the root of 
//  the source distribution for the version number of this OpenEars package.


#import "OELanguageModelGenerator.h"
#import "OEGrammarGenerator.h"
#import "OEGraphemeGenerator.h"
#import "OECMUCLMTKModel.h"

#import "OERuntimeVerbosity.h"
#import "OEAcousticModel.h"


@interface OELanguageModelGenerator ()

@property (nonatomic, strong) NSMutableDictionary *acousticModelCache;
@property (nonatomic, assign) BOOL breakLookups;

@end

@implementation OELanguageModelGenerator

extern int verbose_cmuclmtk;
extern int openears_logging;

static NSString * const kIntrawordCharactersString = @"-'"; // Characters that we leave in place if they are intraword, otherwise automatically remove.
static NSString * const kIgnoredCharactersString = @"!?.,:; "; // Characters which we do not automatically remove, instead automatically leaving them in place.
static NSString * const kSmartQuoteCharactersString = @"‘’"; // Smart quote types which need to be fixed before other processing.
static NSString * const kLanguageModelGeneratorLookupListName = @"LanguageModelGeneratorLookupList.text";
static NSString * const kG2pPlistFileName = @"g2p";

- (instancetype) init {
    if (self = [super init]) {
        _iterationStorageArray = [[NSMutableArray alloc] init];
        _graphemeGenerator = [[OEGraphemeGenerator alloc] init];
        _acousticModelCache = [[NSMutableDictionary alloc] init];
        _breakLookups = FALSE;
    }
    return self;
}

- (NSString *) pathToCachesDirectory {
    return [NSString stringWithFormat:@"%@", NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES)[0]];
}

- (NSArray *) compactWhitespaceAndFixCharactersOfArrayEntries:(NSArray *)array {
    
    NSMutableCharacterSet *interestingCharacters = [[NSMutableCharacterSet alloc] init];
    [interestingCharacters addCharactersInString:[NSString stringWithFormat:@"%@%@%@", kSmartQuoteCharactersString, kIntrawordCharactersString, kIgnoredCharactersString]]; // Operations here are only done on this set, so if this set isn't in the array, we send it back early.
    if ([[array componentsJoinedByString:@""] rangeOfCharacterFromSet:interestingCharacters].location == NSNotFound) return array; // If none of the characters we're normalizing are in this array anywhere, just send it back, we're good.
    
    // Otherwise,
    
    NSMutableArray *storageArray = [NSMutableArray new];
    
    NSMutableCharacterSet *lettersNumbersAndIgnoredSymbols = [NSMutableCharacterSet alphanumericCharacterSet];
    [lettersNumbersAndIgnoredSymbols addCharactersInString:[NSString stringWithFormat:@"%@%@", kIntrawordCharactersString, kIgnoredCharactersString]]; // A character set comprising letters, numbers, intraword characters we remove in a special way, and characters we can leave in place.
    NSCharacterSet *lettersNumbersAndIgnoredSymbolsInvertedSet = [lettersNumbersAndIgnoredSymbols invertedSet];
    NSCharacterSet *whitespaceAndNewlineCharacterSet = [NSCharacterSet whitespaceAndNewlineCharacterSet];
    
    for (NSString *string in array) {
        
        if ([string rangeOfCharacterFromSet:interestingCharacters].location != NSNotFound) {
        
            NSString *text = string;
            if ([text rangeOfString:@" "].location != NSNotFound) {
                text = [text stringByTrimmingCharactersInSet:whitespaceAndNewlineCharacterSet]; 
                NSArray *componentArray = [text componentsSeparatedByCharactersInSet:whitespaceAndNewlineCharacterSet];
                componentArray = [componentArray filteredArrayUsingPredicate:[NSPredicate predicateWithFormat:@"self <> ''"]];
                text = [componentArray componentsJoinedByString:@" "];
            }
                            
            text = [[text stringByReplacingOccurrencesOfString:@"‘" withString:@"'"]stringByReplacingOccurrencesOfString:@"’" withString:@"'"]; // Make any smart quotes into straight quotes automatically before handling apostrophes.
            text = [text stringByReplacingOccurrencesOfString:@"--" withString:@"–"]; // Make fake m-dashes into real m-dashes before handling hyphens.
            
            text = [[text componentsSeparatedByCharactersInSet:lettersNumbersAndIgnoredSymbolsInvertedSet] componentsJoinedByString:@" "]; // Remove all symbols which aren't in the character set above.
            
            text = [self removeAllButIntrawordOccurrencesOfCharacter:@"-" inText:text]; // Remove dashes/hyphens, unless they are found between two letters.
            
            text = [self removeAllButIntrawordOccurrencesOfCharacter:@"'" inText:text]; // Remove apostrophes, unless they are found between two letters, which we now know will only be in the form of straight quotes.
            if ([text rangeOfString:@" "].location != NSNotFound) {
                text = [text stringByReplacingOccurrencesOfString:@" ." withString:@"."]; // Remove space-prepended punctuation, yuck.
                text = [text stringByReplacingOccurrencesOfString:@" ?" withString:@"."]; 
                text = [text stringByReplacingOccurrencesOfString:@" !" withString:@"."]; 
                text = [text stringByReplacingOccurrencesOfString:@" ," withString:@"."]; 
                text = [text stringByReplacingOccurrencesOfString:@" :" withString:@"."]; 
                text = [text stringByReplacingOccurrencesOfString:@" ;" withString:@"."]; 
            }
            if ([text rangeOfCharacterFromSet:[NSCharacterSet alphanumericCharacterSet]].location != NSNotFound) { // If this array contains any letters or numbers, add it back in, otherwise don't – it's blank or just has characters.
                [storageArray addObject:text];
            }
        } else {
            [storageArray addObject:string];
        }
    }

    return storageArray;
}

- (NSString *) removeAllButIntrawordOccurrencesOfCharacter:(NSString *)character inText:(NSString *)text {

    NSArray *textArray = [text componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:character]]; // Separate segments around this character
    
    NSMutableArray *mutableTextArray = [NSMutableArray new]; // Store processed segments
    NSCharacterSet *letterCharacterSet = [NSCharacterSet letterCharacterSet];
    for (NSString *segment in textArray) {
        
        NSString *nextSegment = nil;
        
        if (([textArray indexOfObject:segment] + 1) < [textArray count]) { // If there is a next segment, store it, otherwise fake next segment.
            nextSegment = [textArray objectAtIndex:[textArray indexOfObject:segment] + 1];
        } else {
            nextSegment = @"";
        }
        
        if ( // If there's a character at the end of this segment and a character at the start of the next segment and they're both letters we found an intraword character.
           [segment length] > 0 
           && 
           [nextSegment length] > 0
           &&
           ![segment isEqual:[textArray lastObject]] 
           && 
           [[segment substringFromIndex:[segment length] - 1] rangeOfCharacterFromSet:letterCharacterSet].location != NSNotFound 
           && 
           [[nextSegment substringFromIndex:0] rangeOfCharacterFromSet:letterCharacterSet].location != NSNotFound
           ) {
            [mutableTextArray addObject:[NSString stringWithFormat:@"%@%@",segment,character]]; // This is an intraword character, so add it to the end of the segment so it remains when we reassemble.
        } else {
            [mutableTextArray addObject:segment]; // If there isn't a word with a letter in front and in back, it isn't intraword, just add it back with the special character still absent.
        }
    }
   
    return [mutableTextArray componentsJoinedByString:@""]; // Rejoin everything without the special character (other than the ones we added when they were intraword).
}

- (NSString *) lookupListForAcousticModelAtPath:(NSString *)acousticModelPath {

    NSString *lookupListPath = [acousticModelPath stringByAppendingPathComponent:kLanguageModelGeneratorLookupListName];
    NSString *acousticModelName = [self nameOfAcousticModelAtPath:acousticModelPath];
    NSString *lookupListToReturn = [[self.acousticModelCache objectForKey:acousticModelName]objectForKey:kLanguageModelGeneratorLookupListName];
    if (lookupListToReturn) {
        if (openears_logging == 1) NSLog(@"Returning a cached version of %@", kLanguageModelGeneratorLookupListName);
        return lookupListToReturn; // If this was cached previously, just return the cached version.
    }
    
    // If we get here, there isn't a cached version so we read it in.
    
    if (openears_logging == 1) NSLog(@"Since there is no cached version, loading the language model lookup list for the acoustic model called %@", acousticModelName);
    
    NSError *error = nil;
    lookupListToReturn = [[NSString alloc] initWithContentsOfFile:lookupListPath encoding:NSUTF8StringEncoding error:&error];
    if (error || !lookupListToReturn) {
        NSString *existenceBoilerplate = @"This file appears to exist, so it may have formatting problems.";
        if (![[NSFileManager defaultManager] fileExistsAtPath:lookupListPath]) existenceBoilerplate = @" This file does not appear to exist.";
        NSLog(@"Error: an attempt was made to load the lookup dictionary for the acoustic model at the path %@ and it wasn't possible to complete. %@ Please ask for help in the forums and be sure to turn on all logging. An exception or unpredictable behavior should be expected now since this file is a requirement.", acousticModelPath, existenceBoilerplate);   
        if (error) NSLog(@"Additionally, there was an error of %@", error);
    } else {
        if (![self.acousticModelCache objectForKey:acousticModelName]) { // If this acoustic model has never had a key cached, create one.
            [self.acousticModelCache setObject:[NSMutableDictionary new] forKey:acousticModelName];
        }
        
        if (![[self.acousticModelCache objectForKey:acousticModelName]objectForKey:kLanguageModelGeneratorLookupListName]) { // If no language model generator lookup list has ever been added to this acoustic model cache, add this one.
            [[self.acousticModelCache objectForKey:acousticModelName]setObject:lookupListToReturn forKey:kLanguageModelGeneratorLookupListName];
        }
    }

    return [[self.acousticModelCache objectForKey:acousticModelName]objectForKey:kLanguageModelGeneratorLookupListName]; // If this is ever unreliable, I want it to break early and obviously. 
}

- (NSMutableDictionary *) g2pDictionaryForAcousticModelAtPath:(NSString *)acousticModelPath {
    
    NSString *acousticModelName = [self nameOfAcousticModelAtPath:acousticModelPath]; // Look at name of model

    if ([acousticModelName isEqualToString:@"AcousticModelEnglish"] || [acousticModelName isEqualToString:@"AcousticModelAlternateEnglish1"] || [acousticModelName isEqualToString:@"AcousticModelAlternateEnglish2"]) return nil; // These shouldn't load g2p
    
    NSString *g2pPlistPath = [acousticModelPath stringByAppendingPathComponent:kG2pPlistFileName]; // append g2p
    NSMutableDictionary *dictionaryToReturn = [[self.acousticModelCache objectForKey:acousticModelName]objectForKey:@"g2pModel"];
    if (dictionaryToReturn) {
        if (openears_logging == 1) NSLog(@"Returning a cached version of %@", kG2pPlistFileName);
        return dictionaryToReturn; // If this was cached previously, just return the cached version.
    }
    
    // If we get here, there isn't a cached version so we read it in.
    
    if (openears_logging == 1) NSLog(@"Since there is no cached version, loading the g2p model for the acoustic model called %@", acousticModelName);
    
    dictionaryToReturn = [NSMutableDictionary dictionaryWithContentsOfFile:g2pPlistPath]; // Grab g2p model
    if (!dictionaryToReturn) { 
        NSString *existenceBoilerplate = @"This file appears to exist, so it may have formatting problems.";
        if (![[NSFileManager defaultManager] fileExistsAtPath:g2pPlistPath]) existenceBoilerplate = @" This file does not appear to exist.";
        NSLog(@"Error: an attempt was made to load the g2p file for the acoustic model at the path %@ and it wasn't possible to complete. %@ Please ask for help in the forums and be sure to turn on all logging. An exception or unpredictable behavior should be expected now since this file is a requirement.",acousticModelPath,existenceBoilerplate);   

    } else {
        if (![self.acousticModelCache objectForKey:acousticModelName]) { // If this acoustic model has never had a key cached, create one.
            [self.acousticModelCache setObject:[NSMutableDictionary new] forKey:acousticModelName];
        }
        
        if (![[self.acousticModelCache objectForKey:acousticModelName]objectForKey:@"g2pModel"]) { // If no g2p model has ever been added to this acoustic model cache, add this one.
            [[self.acousticModelCache objectForKey:acousticModelName]setObject:dictionaryToReturn forKey:@"g2pModel"];
        }
    }

    return [[self.acousticModelCache objectForKey:acousticModelName]objectForKey:@"g2pModel"]; 
}

- (NSString *) nameOfAcousticModelAtPath:(NSString *)acousticModelPath {
    NSString *name = [[acousticModelPath lastPathComponent] stringByReplacingOccurrencesOfString:@".bundle" withString:@""]; // Canonical name please.
    return name;
}

- (NSNumber *) testG2PForAcousticModel:(NSDictionary *)testDictionary {
    
    NSTimeInterval lookupTime = [NSDate timeIntervalSinceReferenceDate];
    
    NSString *acousticModelPath = [testDictionary objectForKey:@"AcousticModelPath"];
    BOOL logRerunsForLargeOffsets = [[testDictionary objectForKey:@"LogRerunsForLargeOffsets"]boolValue];
    BOOL verbose = [[testDictionary objectForKey:@"Verbose"]boolValue];   
    BOOL displayNonHitsWithNoOffsets = [[testDictionary objectForKey:@"DisplayNonHitsWithNoOffsets"]boolValue];   
    BOOL breakingLookups = [[testDictionary objectForKey:@"BreakingLookups"]boolValue]; 
    
    if (breakingLookups) self.breakLookups = TRUE;
    
    NSString *lookupListAsString = [self lookupListForAcousticModelAtPath:acousticModelPath];

    NSArray *lookupListArray = [lookupListAsString componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]];
  
    
    NSMutableArray *easyLines = [[NSMutableArray alloc] init];
    
    NSMutableDictionary *lookupDictionary = [self g2pDictionaryForAcousticModelAtPath:acousticModelPath];
    
    BOOL unigramsOnly = FALSE;
    if ([lookupDictionary objectForKey:@"UnigramsOnly"])unigramsOnly = [[lookupDictionary objectForKey:@"UnigramsOnly"]boolValue];
    
    for (NSString *line in lookupListArray) {
        NSArray *partsArray = [line componentsSeparatedByString:@"\t"];
        if (([partsArray count] == 2) && 
           ([partsArray[0] rangeOfString:@"-"].location == NSNotFound && [partsArray[0] rangeOfString:@"("].location == NSNotFound && [partsArray[0] rangeOfString:@"_"].location == NSNotFound)) [easyLines addObject:line];
    }

    
    NSInteger hits = 0;
    NSInteger nonHits1 = 0;
    NSInteger nonHits2 = 0;
    NSInteger nonHits3 = 0;
    NSInteger nonHits4 = 0;
    NSInteger nonHits5 = 0;    
    NSInteger total = [easyLines count];
    NSInteger excessiveOffset = 5;
    NSString *g2pResults = nil;

    
    for (NSString *line in easyLines) {
        
        NSArray *partsArray = [line componentsSeparatedByString:@"\t"];

        g2pResults = [self.graphemeGenerator getPhonemesForWordUsingGeneralizedG2P:partsArray[0] usingG2PDictionary:lookupDictionary forAcousticModelNamed:[self nameOfAcousticModelAtPath:acousticModelPath] usingUnigramsOnly:unigramsOnly verbose:verbose];

        if ([[[partsArray[1] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]lowercaseString] isEqualToString: [[g2pResults stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]lowercaseString]]) {
            hits++;
        } else {

            NSInteger numberOfOriginalPhonemes = [[partsArray[1] componentsSeparatedByString:@" "]count];
            NSInteger numberOfGeneratedPhonemes = [[g2pResults componentsSeparatedByString:@" "]count];
            NSInteger difference = (NSInteger)llabs(numberOfOriginalPhonemes-numberOfGeneratedPhonemes);
            if (difference > excessiveOffset) {

                if (logRerunsForLargeOffsets) {
                    NSLog(@"verbose re-run follows:");
                    NSLog(@"\"%@\"\t\"%@\"",partsArray[0], [self.graphemeGenerator getPhonemesForWordUsingGeneralizedG2P:partsArray[0] usingG2PDictionary:lookupDictionary forAcousticModelNamed:[self nameOfAcousticModelAtPath:acousticModelPath]  usingUnigramsOnly:unigramsOnly verbose:TRUE]);
                }
            }
            if (difference == 0) {
                if (displayNonHitsWithNoOffsets) {
                    NSLog(@"original word is \"%@\" and its pronunciation is \"%@\" and the g2p result is \"%@\"", partsArray[0],partsArray[1], g2pResults);
                }
            }
            if (difference == 1) nonHits1++;
            if (difference == 2) nonHits2++;
            if (difference == 3) nonHits3++;
            if (difference == 4) nonHits4++;
            if (difference == 5) nonHits5++;
        }

    }
    
    if (hits == 0)hits++; // No divisors of zero please.
    if (nonHits1 == 0) nonHits1++;
    if (nonHits2 == 0) nonHits2++;
    if (nonHits3 == 0) nonHits3++;
    if (nonHits4 == 0) nonHits4++;
    if (nonHits5 == 0) nonHits5++;
    
    float hitPercentage = 100.0 / (float)((float)total / (float)hits);
    float nonHit1Percentage = 100.0 / ((float)((float)total - (float)hits) / (float)nonHits1);
    float nonHit2Percentage = 100.0 / ((float)((float)total - (float)hits) / (float)nonHits2);
    float nonHit3Percentage = 100.0 / ((float)((float)total - (float)hits) / (float)nonHits3);
    float nonHit4Percentage = 100.0 / ((float)((float)total - (float)hits) / (float)nonHits4);
    float nonHit5Percentage = 100.0 / ((float)((float)total - (float)hits) / (float)nonHits5);
    float nonHit0Percentage = 100.0 - ((float)nonHit1Percentage + (float)nonHit2Percentage + (float)nonHit3Percentage + (float)nonHit4Percentage + (float)nonHit5Percentage);
    float nonHit0PercentageAsOverallPercentage = ((float)(100.0 - (float)hitPercentage)) * (nonHit0Percentage * .01);
    printf("\n####################################################################################");
    printf("\n####################################################################################");
    printf("\nHits are %lu and count is %lu for an accuracy of %.2f%%.\n\n%.2f%% of non-hits had no offsets, %.2f%% of non-hits had an offset of 1, %.2f%% of non-hits had an offset of 2,\n%.2f%% of non-hits had an offset of 3, %.2f%% of non-hits had an offset of 4,\n%.2f%% of non-hits had an offset of 5. Passable recognition percentage may be in the neighborhood of %.2f%%.\n\nTime was an average of %f seconds per word.\n", (unsigned long)hits, (unsigned long)total, hitPercentage, nonHit0Percentage, nonHit1Percentage, nonHit2Percentage, nonHit3Percentage, nonHit4Percentage, nonHit5Percentage, (hitPercentage + nonHit0PercentageAsOverallPercentage), ([NSDate timeIntervalSinceReferenceDate] - lookupTime) / total); 
    printf("\n####################################################################################");
    printf("\n####################################################################################\n\n");
    
    if (breakingLookups) self.breakLookups = FALSE;
    
    return @(hitPercentage);
}

- (NSArray *) performDictionaryLookup:(NSArray *)array inString:(NSString *)stringInput forAcousticModelAtPath:(NSString *)acousticModelPath {

    if (self.breakLookups) {
        stringInput = @" "; // Bogus lookuplist string that will (almost) always fail for when we're testing g2p.
        NSLog(@"breakLookups is TRUE, which means that every word will have its pronunciation generated using the fallback method. This is a testing feature only and if it is appearing in production or app runtime, it is a bug which will negatively affect recognition accuracy. If you are receiving this log output as a user of the API please file a report, explaining modifications you've made to the library if you've made any. If this output appears in tests which are not expecting to only test fallback g2p methods, it is probably a mistake and the results of the test are probably unreliable.");
    }
    

    NSMutableDictionary *lookupDictionary = [self g2pDictionaryForAcousticModelAtPath:acousticModelPath];
    
    if ((!lookupDictionary && [acousticModelPath rangeOfString:@"English"].location == NSNotFound) && openears_logging == 1) NSLog(@"Error: a %@ is missing in a case where one will be needed. Expect an exception shortly. If you need help getting a new acoustic model set up with a %@ please come by the forums and inquire.", kG2pPlistFileName, kG2pPlistFileName);
    
    NSMutableArray *mutableArrayOfWordsToMatch = [[NSMutableArray alloc] initWithArray:array];
    
    NSUInteger position = 0;
    NSInteger preSearchBuffer = 0;
    
    NSMutableArray *matches = [NSMutableArray array];
    
    NSString *lastFoundFirstCharacter = @"";
    
    while (position != stringInput.length) {
        
        if ([mutableArrayOfWordsToMatch count] <= 0) { // If we're at the top of the loop without any more words, stop.
            break;
        }  

        preSearchBuffer = 2000;
        
        if (preSearchBuffer > position) preSearchBuffer = position; // We want to be able to re-search a range if we have multiple words which should find the same entry.        
        
        NSString *stringToSearch = [[mutableArrayOfWordsToMatch[0] stringByTrimmingCharactersInSet:[[NSCharacterSet alphanumericCharacterSet]invertedSet]]lowercaseString]; // Searching is always done with a lowercase string
        
        NSInteger lengthOfSearch = 0;
        
        NSInteger spanOfOneLetterInLargeLookupDictionary = 350000;
        
        if ([stringToSearch hasPrefix:lastFoundFirstCharacter] && stringInput.length - position > spanOfOneLetterInLargeLookupDictionary) { // If we're still looking up the same first letter as the last word, we don't need to search the entire string forward – 350000 is enough characters until that first letter changes.
            lengthOfSearch = spanOfOneLetterInLargeLookupDictionary;
        } else {
            lengthOfSearch = stringInput.length - position; // However, if there are fewer than 350000 characters remaining in the string, we'll just search through to the end of the string.
        }
        
        NSRange remaining = NSMakeRange(position - preSearchBuffer, lengthOfSearch);
        NSRange rangeOfCurrentSearch = [stringInput // this doesn't need to be lowercased because all lookup lists are required to be lowercased in advance.
                                        rangeOfString:[NSString stringWithFormat:@"\n%@\t", stringToSearch]
                                        options:NSLiteralSearch
                                        range:remaining
                                        ]; // Just search for the first pronunciation.
        
        if (rangeOfCurrentSearch.location != NSNotFound) {
            
            lastFoundFirstCharacter = [stringToSearch substringToIndex:1]; // Set the last found character to this character
            
            NSRange lineRange = [stringInput lineRangeForRange:NSMakeRange(rangeOfCurrentSearch.location + 1, rangeOfCurrentSearch.length)];
            
            NSString *matchingLine = [stringInput substringWithRange:NSMakeRange(lineRange.location, lineRange.length - 1)];
            
            NSArray *matchingLineHalves = [matchingLine componentsSeparatedByString:@"\t"];
            
            NSString *matchingLineFixed = [NSString stringWithFormat:@"%@\t%@", mutableArrayOfWordsToMatch[0], matchingLineHalves[1]];
            
            [matches addObject:matchingLineFixed]; // Grab the whole line of the hit but with the original entry replaced by the original request.
            
            NSInteger rangeLocation = rangeOfCurrentSearch.location;
            NSInteger rangeLength = 2000;
            
            if (stringInput.length - rangeOfCurrentSearch.location < rangeLength) { // Only use the searchPadding if there is that much room left in the string.
                rangeLength = stringInput.length - rangeOfCurrentSearch.location;
            } 

            NSInteger newlocation = rangeLocation;
            NSInteger lastlocation = newlocation;
            
            for (int pronunciationAlternativeNumber = 2; pronunciationAlternativeNumber < 6; pronunciationAlternativeNumber++) { // We really only need to do this from 2-5.
                NSRange morematches = [stringInput
                                       rangeOfString:[NSString stringWithFormat:@"\n%@(%d", stringToSearch, pronunciationAlternativeNumber]
                                       options:NSLiteralSearch
                                       range:NSMakeRange(newlocation, (rangeLength - (newlocation - lastlocation)))
                                       ];
                if (morematches.location != NSNotFound) {
                    NSRange moreMatchesLineRange = [stringInput lineRangeForRange:NSMakeRange(morematches.location + 1, morematches.length)]; // Plus one because I don't actually want the line break at the beginning.
                    
                    NSString *moreMatchesLine = [stringInput substringWithRange:NSMakeRange(moreMatchesLineRange.location, moreMatchesLineRange.length - 1)];
                    
                    NSArray *moreMatchesLineHalves = [moreMatchesLine componentsSeparatedByString:@"("];
                    
                    NSString *moreMatchesLineFixed = [NSString stringWithFormat:@"%@(%@", mutableArrayOfWordsToMatch[0], moreMatchesLineHalves[1]];
                    
                    [matches addObject:moreMatchesLineFixed]; // Grab the whole line of the hit.
                    lastlocation = newlocation;
                    newlocation = morematches.location;
                    
                } else {
                    break;   
                }
            }
            
            rangeOfCurrentSearch.length = rangeOfCurrentSearch.location - position;
            rangeOfCurrentSearch.location = position;
            [mutableArrayOfWordsToMatch removeObjectAtIndex:0]; // Remove the word.
            position += (rangeOfCurrentSearch.length + 1);
            
        } else { // No hits.
            
            NSString *unmatchedWord = mutableArrayOfWordsToMatch[0];

            NSArray *hyphenateArray = [unmatchedWord componentsSeparatedByString:@"-"];

            NSMutableArray *reconstructedHyphenateArray = [[NSMutableArray alloc] init];
   
            NSString *formattedString = nil;

            if (openears_logging == 1) NSLog(@"The word %@ was not found in the dictionary of the acoustic model %@. Now using the fallback method to look it up. If this is happening more frequently than you would expect, likely causes can be that you are entering words in another language from the one you are recognizing, or that there are symbols (including numbers) that need to be spelled out or cleaned up, or you are using your own acoustic model and there is an issue with either its phonetic dictionary or it lacks a %@ file. Please get in touch at the forums for assistance with the last two possible issues.", unmatchedWord, acousticModelPath, kG2pPlistFileName);
                                   
            BOOL unigramsOnly = FALSE;
            if ([lookupDictionary objectForKey:@"UnigramsOnly"]) {
                unigramsOnly = [[lookupDictionary objectForKey:@"UnigramsOnly"]boolValue];
            }
            
            for (NSString *subhyphenate in hyphenateArray) {
                if (subhyphenate && ![subhyphenate isEqualToString:@""] && ![subhyphenate isEqualToString:@" "] && ![subhyphenate isEqualToString:@"  "])[reconstructedHyphenateArray addObject:[self.graphemeGenerator getPhonemesForWordUsingGeneralizedG2P:subhyphenate usingG2PDictionary:lookupDictionary forAcousticModelNamed:[self nameOfAcousticModelAtPath:acousticModelPath] usingUnigramsOnly:unigramsOnly verbose:FALSE]];
            }

            NSString *reconstructedHyphenate = [reconstructedHyphenateArray componentsJoinedByString:@" "];

            NSMutableArray *whiteSpaceArray = [NSMutableArray arrayWithArray:[reconstructedHyphenate componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
            [whiteSpaceArray removeObject:@""];
            reconstructedHyphenate = [whiteSpaceArray componentsJoinedByString:@" "];

            formattedString = [NSString stringWithFormat:@"%@\t%@", unmatchedWord, reconstructedHyphenate];
            if (openears_logging == 1) NSLog(@"the graphemes \"%@\" were created for the word %@ using the fallback method.", reconstructedHyphenate, unmatchedWord);
            
            NSString *finalizedString = [formattedString stringByReplacingOccurrencesOfString:@" \n" withString:@"\n"];                     
            
            [matches addObject:finalizedString];
            
            [mutableArrayOfWordsToMatch removeObjectAtIndex:0]; // Remove from the word list.
        }
    }    

    return matches;
}

 
- (void) createLanguageModelFromFilename:(NSString *)fileName {
    if (openears_logging == 1) NSLog(@"Starting dynamic language model generation"); 
    
    NSTimeInterval start = 0.0;
    
    if (openears_logging == 1) {
        start = [NSDate timeIntervalSinceReferenceDate]; // If logging is on, let's time the language model processing time so the developer can profile it.
    }
    
    OECMUCLMTKModel *cmuCLMTKModel = [[OECMUCLMTKModel alloc] init]; // First, use the CMUCLMTK port to create a language model
    // -linear | -absolute | -good_turing | -witten_bell
    cmuCLMTKModel.algorithmType = @"-witten_bell";
    
    if (self.ngrams) {
        cmuCLMTKModel.ngrams = self.ngrams;
    }
    
    [cmuCLMTKModel runCMUCLMTKOnCorpusFile:[self.pathToCachesDirectory stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.corpus", fileName]] withBin:TRUE binarySuffix:kBinaryFileSuffix];
    self.ngrams = nil;

#ifdef KEEPFILES
#else    
	NSError *deleteCorpusError = nil;
	NSFileManager *fileManager = [NSFileManager defaultManager]; // Let's make a best effort to erase the corpus now that we're done with it, but we'll carry on if it gives an error.
	[fileManager removeItemAtPath:[self.pathToCachesDirectory stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.corpus", fileName]] error:&deleteCorpusError];
	if (deleteCorpusError != 0) {
		if (openears_logging == 1) NSLog(@"Error while deleting language model corpus: %@", deleteCorpusError);
	}

#endif
    
    if (openears_logging == 1) {
        NSLog(@"Done creating language model with CMUCLMTK in %f seconds.", [NSDate timeIntervalSinceReferenceDate] - start);
    }
}

- (NSError *) checkModelForContent:(NSArray *)normalizedLanguageModelArray {
    if ([normalizedLanguageModelArray count] < 1 || [[normalizedLanguageModelArray componentsJoinedByString:@""]length] < 1) {

		return [NSError errorWithDomain:@"com.politepix.openears" code:6000 userInfo:@{NSLocalizedDescriptionKey: @"Language model has no content."}];
	} 
    return nil;
}

- (NSError *) writeOutCorpusForArray:(NSArray *)normalizedLanguageModelArray toFilename:(NSString *)fileName {
    NSMutableString *mutableCorpusString = [[NSMutableString alloc] initWithString:[normalizedLanguageModelArray componentsJoinedByString:@" </s>\n<s> "]];
    
    [mutableCorpusString appendString:@" </s>\n"];
    [mutableCorpusString insertString:@"<s> " atIndex:0];
    NSString *corpusString = (NSString *)mutableCorpusString;
    NSError *error = nil;
    [corpusString writeToFile:[self.pathToCachesDirectory stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.corpus", fileName]] atomically:YES encoding:NSUTF8StringEncoding error:&error];
    if (error) {
        // Handle error here
        if (openears_logging == 1) NSLog(@"Error: file was not written out due to error %@", error);

        return error;
    }
    
    return nil;
}

- (NSError *) generateLanguageModelFromTextFile:(NSString *)pathToTextFile withFilesNamed:(NSString *)fileName  forAcousticModelAtPath:(NSString *)acousticModelPath {

    NSString *textFile = nil;
    
    // Return an error if we can't read a file at that location at all.
    
    if (![[NSFileManager defaultManager] isReadableFileAtPath:pathToTextFile]) {
        if (openears_logging == 1) NSLog(@"Error: you are trying to generate a language model from a text file at the path %@ but there is no file at that location which can be opened.", pathToTextFile);
    
        return [NSError errorWithDomain:@"com.politepix.openears" code:10020 userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Error: you are trying to generate a language model from a text file at the path %@ but there is no file at that location which can be opened.", pathToTextFile]}];
    } else { // Try to read in the file
        NSError *error = nil;
        textFile = [NSString stringWithContentsOfFile:pathToTextFile encoding:NSUTF8StringEncoding error:&error];
        if (error) return error; // Die if we can't read in this particular file as a string.

    }
    
    NSMutableArray *mutableArrayToReturn = [[NSMutableArray alloc] init];
        
    NSArray *corpusArray = [textFile componentsSeparatedByCharactersInSet:
                            [NSCharacterSet newlineCharacterSet]]; // Create an array from the corpus that is separated by any variety of newlines.
    
    for (NSString *string in corpusArray) { // Fast enumerate through this array
        if ([[string stringByTrimmingCharactersInSet: [NSCharacterSet whitespaceAndNewlineCharacterSet]] length] != 0) { // Only keep strings which consist of more than whitespace or newlines only
            // This string has something in it besides whitespace or newlines
            NSString *trimmedString = [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]; // If we find a string that possesses content, remove whitespace and newlines from its very beginning and very end.

            [mutableArrayToReturn addObject:trimmedString]; // Add it to the array
        } 
    }
    
    
    
    NSArray *arrayToReturn = [NSArray arrayWithArray:mutableArrayToReturn]; // Set this to an immutable object to return

    return [self generateLanguageModelFromArray:arrayToReturn withFilesNamed:fileName forAcousticModelAtPath:acousticModelPath]; // hand off this string to the real method.

}


- (NSError *) generateLanguageModelFromArray:(NSArray *)languageModelArray withFilesNamed:(NSString *)fileName forAcousticModelAtPath:(NSString *)acousticModelPath {

    NSTimeInterval start = [NSDate timeIntervalSinceReferenceDate];

    if (self.verboseLanguageModelGenerator) {
        verbose_cmuclmtk = 1; 
    } else {
        verbose_cmuclmtk = 0;
    }
    
    NSArray *normalizedLanguageModelArray = [self compactWhitespaceAndFixCharactersOfArrayEntries:languageModelArray]; // We are normalizing the array first to get rid of any whitespace other than one single space between two words.
    
    NSError *error = nil; // Used throughout the method

    error = [self checkModelForContent:normalizedLanguageModelArray]; // Make sure this language model has something in it.
    if (error) {

        return error;   
    }
    
    error = [self writeOutCorpusForArray:normalizedLanguageModelArray toFilename:fileName]; // Write the corpus out to the filesystem.
    
    if (error) {
        return error;   
    }
    
    [self createLanguageModelFromFilename:fileName]; // Generate the language model using CMUCLMTK.
    
    NSMutableArray *dictionaryResultsArray = [[NSMutableArray alloc] init];
    
    error = [self createDictionaryFromWordArray:normalizedLanguageModelArray intoDictionaryArray:dictionaryResultsArray usingAcousticModelAtPath:acousticModelPath];
    
    if (!error) {
        // Write out the results array as a dictionary file in the caches directory
        BOOL writeOutSuccess = [[NSString stringWithFormat:@"%@\n", [dictionaryResultsArray componentsJoinedByString:@"\n"]] writeToFile:[self.pathToCachesDirectory stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.dic", fileName]] atomically:YES encoding:NSUTF8StringEncoding error:&error];

        if (!writeOutSuccess) { // If this fails, return an error.
            if (openears_logging == 1) NSLog(@"Error writing out dictionary: %@", error);		
            return error;
        } 
        
    } else {
        return [NSError errorWithDomain:@"com.politepix.openears" code:6001 userInfo:@{NSLocalizedDescriptionKey: @"Not possible to create a dictionary for this wordset."}];    
    }
    
    if (openears_logging == 1) NSLog(@"I'm done running dynamic language model generation and it took %f seconds", [NSDate timeIntervalSinceReferenceDate] - start); // Deliver the timing info if logging is on.
    
    return nil;
}

- (NSDictionary *) renameKey:(id)originalKey to:(id)newKey inDictionary:(NSDictionary *)dictionary {
    
    NSMutableDictionary *tempMutableDictionary = [[NSMutableDictionary alloc] initWithDictionary:dictionary];
    
    id value = tempMutableDictionary[originalKey];
    [tempMutableDictionary removeObjectForKey:originalKey];
    tempMutableDictionary[newKey] = value;
    
    return (NSDictionary *)tempMutableDictionary;
}

- (NSString *) pathToSuccessfullyGeneratedDictionaryWithRequestedName:(NSString *)name {
    return [NSString stringWithFormat:@"%@/%@.%@", self.pathToCachesDirectory, name, @"dic"];
}

- (NSString *) pathToSuccessfullyGeneratedLanguageModelWithRequestedName:(NSString *)name {

    return [NSString stringWithFormat:@"%@/%@.%@", self.pathToCachesDirectory, name, kBinaryFileSuffix];    
}

- (NSString *) pathToSuccessfullyGeneratedGrammarWithRequestedName:(NSString *)name {
    return [NSString stringWithFormat:@"%@/%@.%@", self.pathToCachesDirectory, name, @"gram"];
}

- (NSError *) createDictionaryFromWordArray:(NSArray *)normalizedLanguageModelArray intoDictionaryArray:(NSMutableArray *)dictionaryResultsArray usingAcousticModelAtPath:(NSString *)acousticModelPath {
    
    NSString *allWords = [normalizedLanguageModelArray componentsJoinedByString:@" "]; // Grab all the words in question
    
    NSArray *allEntries = [allWords componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]; // Make an array entry for anything separated by a whitespace symbol
    
    NSArray *arrayWithNoDuplicates = [[NSSet setWithArray:allEntries] allObjects]; // Remove duplicate words through the magic of NSSet
    
    NSArray *sortedArray = [arrayWithNoDuplicates sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]; // Alphabetic sort
    
    // load the dictionary file, whatever it is.    
    NSString *pronunciationDictionaryString = [self lookupListForAcousticModelAtPath:acousticModelPath];
    
    if (!pronunciationDictionaryString) {
        
        return [NSError errorWithDomain:@"com.politepix.openears" code:1002 userInfo:@{
                                                                                       NSLocalizedDescriptionKey: @"Not possible to load the language model lookup list file for the acoustic model at the path %@, exiting early.",
                                                                                       NSLocalizedFailureReasonErrorKey: @"Not possible to load the language model lookup list file for the acoustic model at the path %@, exiting early.",
                                                                                       NSLocalizedRecoverySuggestionErrorKey: @"Not possible to load the language model lookup list file for the acoustic model at the path %@, exiting early.",
                                                                                       }];   
    }
    
    NSTimeInterval performDictionaryLookupTime = 0.0; // We'll time this operation since it's critical.
    
    if (openears_logging == 1) {
        performDictionaryLookupTime = [NSDate timeIntervalSinceReferenceDate];
    }
    
    [dictionaryResultsArray addObjectsFromArray:[self performDictionaryLookup:sortedArray inString:pronunciationDictionaryString forAcousticModelAtPath:acousticModelPath]];// Do the dictionary pronunciation lookup

    if (openears_logging == 1) NSLog(@"I'm done running performDictionaryLookup and it took %f seconds", [NSDate timeIntervalSinceReferenceDate] - performDictionaryLookupTime);
    
    return nil;
    
}

- (NSError *) generateGrammarFromDictionary:(NSDictionary *)grammarDictionary withFilesNamed:(NSString *)fileName forAcousticModelAtPath:(NSString *)acousticModelPath {
    
    NSDictionary *fixedGrammarDictionary = [self renameKey:[[grammarDictionary allKeys] firstObject] to:[NSString stringWithFormat:@"PublicRule%@", [[grammarDictionary allKeys] firstObject]] inDictionary:grammarDictionary];
    
    NSMutableArray *phoneticDictionaryArray = [[NSMutableArray alloc] init];
    
    OEGrammarGenerator *grammarGenerator = [[OEGrammarGenerator alloc] init];
    
    grammarGenerator.delegate = self;
    grammarGenerator.acousticModelPath = acousticModelPath;
    
    NSError *error = [grammarGenerator createGrammarFromDictionary:fixedGrammarDictionary withRequestedName:fileName creatingPhoneticDictionaryArray:phoneticDictionaryArray];
    
    if (error) {
        NSLog(@"It wasn't possible to create this grammar: %@", grammarDictionary);
        error = [NSError errorWithDomain:@"com.politepix.openears" code:10040 userInfo:@{NSLocalizedDescriptionKey: @"It wasn't possible to generate a grammar for this dictionary, please turn on OELogging for more information"}];
    }
    
    return error;
}



@end
