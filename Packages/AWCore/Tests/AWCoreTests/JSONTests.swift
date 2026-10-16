import XCTest
@testable import AWCore

final class JSONTests: XCTestCase {
    /// `js-format-cases.json` holds doubles (as bit patterns) and strings with what Node's JSON.stringify wrote.
    struct Cases: Decodable {
        let numbers: [[String]]
        let strings: [[String]]
    }

    func cases() throws -> Cases {
        let url = try XCTUnwrap(Bundle.module.url(forResource: "js-format-cases", withExtension: "json", subdirectory: "Fixtures"))
        return try JSONValueDecoder().decode(Cases.self, from: Data(contentsOf: url))
    }

    func testNumbersFormatLikeJavaScript() throws {
        for c in try cases().numbers {
            let x = Double(bitPattern: try XCTUnwrap(UInt64(c[0], radix: 16)))
            XCTAssertEqual(JSONWriter.formatNumber(x), c[1], "bits \(c[0]) (\(x))")
        }
    }

    func testStringsEscapeLikeJavaScript() throws {
        for c in try cases().strings {
            XCTAssertEqual(JSONWriter.string(.string(c[0])), c[1])
        }
    }

    func testSpecialNumbers() {
        XCTAssertEqual(JSONWriter.formatNumber(.nan), "null")
        XCTAssertEqual(JSONWriter.formatNumber(.infinity), "null")
        XCTAssertEqual(JSONWriter.formatNumber(-0.0), "0")
        XCTAssertEqual(JSONWriter.formatNumber(1e21), "1e+21")
        XCTAssertEqual(JSONWriter.formatNumber(1e-7), "1e-7")
        XCTAssertEqual(JSONWriter.formatNumber(0.000001), "0.000001")
        XCTAssertEqual(JSONWriter.formatNumber(123.456), "123.456")
    }

    func testObjectsKeepKeyOrderAndDuplicateKeysBehaveLikeJSONParse() throws {
        let v = try JSONParser.parse(#"{"z":1,"a":2,"z":3}"#)
        XCTAssertEqual(JSONWriter.string(v), #"{"z":3,"a":2}"#)
    }

    func testParserAcceptsWhitespaceAndEscapes() throws {
        let v = try JSONParser.parse(" { \"a\" : [ 1 , -2.5e3 , true , null , \"\\u00e9\\ud83d\\udc1b\\/\" ] } \n")
        XCTAssertEqual(JSONWriter.string(v), #"{"a":[1,-2500,true,null,"é🐛/"]}"#)
    }

    func testParserRejectsBadInput() {
        for bad in ["", "{", "[1,]", "{\"a\" 1}", "01", "1.", "\"\u{01}\"", "nul", "[1] x"] {
            XCTAssertThrowsError(try JSONParser.parse(bad), bad)
        }
    }
}
