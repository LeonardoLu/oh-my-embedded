import Foundation

// Run the same source-level validators against repository fixtures outside the GUI sandbox.
@main
struct CoralliumFixtureTests {
    @MainActor static func main() throws {
        try CoralliumSelfTests.run()
        print("Corallium repository fixture tests: PASS")
    }
}
