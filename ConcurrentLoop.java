import java.util.*;

class ConcurrentLoop {
  private List<List<String>> collections;
  private List<Integer> indexes;
  private List<Integer> current;
  private List<Integer> last;
  private List<Integer> tallies;
  private List<Integer> tallyCurrent;
  private List<Integer> totalIndexes;
  private List<Integer> nextIndexes;
  private boolean alternate;
  private int position;
  private int positionRaw;
  private boolean shifting = false;
  boolean allMax = true;
  private Func func;
  private int currentPos = 0;
  
  public ConcurrentLoop(List<List<String>> collections, Func func) {
    this.alternate = false;
    this.collections = collections;
    this.func = func;
    this.indexes = new ArrayList<>();
    this.last = new ArrayList<>();
    this.current = new ArrayList<>();
    this.nextIndexes = new ArrayList<>();
    this.tallies = new ArrayList<>();
    this.totalIndexes = new ArrayList<>();
    this.tallyCurrent = new ArrayList<>();
  
    this.position = 0;
    this.positionRaw = 0;
    for (int i = 0 ; i < collections.size(); i++) {
      this.indexes.add(0);
      this.tallies.add(0);
      this.tallyCurrent.add(0);
      this.totalIndexes.add(0);
      this.last.add(0);
      this.current.add(0);
      
    }
  }

  public interface Func {
    public String run(List<String> slice);
  }

  public int size() {
    int total = collections.get(0).size();
    for (int i = 1 ; i < collections.size(); i++) {
      total = total * collections.get(i).size();
    }
    return total;
  }
  
  public String tick() {
    boolean preservePosition = false;
    for (int i = 0 ; i < this.indexes.size(); i++) {
      
      
      if ((this.indexes.get(i))  >= this.collections.get(i).size()) {
        System.out.println("Wrap around");
        this.tallies.set(i, (this.tallies.get(i)+1));
       
      }
      
      
      
        this.indexes.set(i, ((this.indexes.get(i)) % this.collections.get(i).size()));
      
      
    }
    List<String> items = new ArrayList<String>();
    for (int i = 0 ; i < this.collections.size(); i++) {
      
      String item = this.collections.get(i).get(this.indexes.get(i));
      items.add(item);
    }

    if (!shifting) {
      allMax = true;
      for (int i = 0 ; i < this.indexes.size() ; i++) {
        if (this.indexes.get(i) != this.collections.get(i).size() - 1) {
          allMax = false;
          System.out.println(this.indexes.get(i));
          System.out.println("All max false");
        }
      
      }
    }

    if (!shifting && allMax) {
      int current = 0;
      for (int i = 0 ; i < this.indexes.size(); i++) {
        this.indexes.set(i, current++);
      }
      shifting = true;
    }

    if (shifting) {
	  System.out.println(this.indexes);
      for (int i = 0 ; i < this.indexes.size(); i++) {
        int previousIndex = i - 1;
       	boolean overwritePrevious = false; 
		int previousValue;
		int nextValue;
        if (i == 0) { overwritePrevious = true;  previousIndex = this.indexes.size() - 1; previousValue = this.collections.get(i).size(); nextValue = this.indexes.get(i + 1); }
        else if (i == this.indexes.size() - 1) { overwritePrevious = false; previousValue = this.indexes.get(previousIndex); nextValue = (this.indexes.get(i - 1) + 1) % this.collections.get(i - 1).size(); }
		else {
			overwritePrevious = false;
			previousValue = this.indexes.get(previousIndex);
			nextValue = this.indexes.get(i + 1);
		}
		if (overwritePrevious) {
			// this.indexes.set(previousIndex, previousValue);
		}
        this.indexes.set(i, nextValue);
		System.out.println(this.indexes);
        
      }
	  System.out.println(this.indexes);
    } else {
    
    for (int i = 0 ; i < this.indexes.size() ; i++) {
        
        

        
      
      int newValue = (this.current.get(i) + 1);
      if (newValue > this.collections.get(i).size()) {
            tallies.set(i, tallies.get(i) + 1); 
      }
      newValue = newValue % this.collections.get(i).size();
      this.current.set(i, newValue);
      this.indexes.set(i, newValue);
      
        
      
      this.tallyCurrent.set(i, this.tallyCurrent.get(i)+1);
      this.totalIndexes.set(i, this.totalIndexes.get(i) + 1);
      
      
        
        
        
      
    }
	
    for (int i = 0 ; i < this.indexes.size() ; i++) {
      
      if (this.tallies.get(i) > 0 && this.tallyCurrent.get(i) >= tallies.get(i)) {
        
          
        
        
          
        
        
          
        
      }
      if (this.position % 2 == 0) {
        this.indexes.set(i, (this.indexes.get(i) + this.tallies.get(i)));
      }
      
    }
	}
    if (!preservePosition) {
      position = (position+1) % this.indexes.size();
    }
    positionRaw = positionRaw + 1;
	
    return this.func.run(items);
  }
  
  public static void main(String[] args) {
    List<String> letters = Arrays.asList(new String[]{"a", "b", "c", "d" /*, "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z" */});
    List<String> numbers = Arrays.asList(new String[] {"1", "2", "3", "4"});
    List<String> symbols = Arrays.asList(new String[] { "÷", "×", "(", ")", "&"});
    List<Integer> chunkSize = Arrays.asList(new Integer[] { 2, 1, 1});
    List<String> concurrent = new ArrayList<>();
    List<String> iterative = new ArrayList<>();
    int total = letters.size() * numbers.size() * symbols.size();
    int a = 0; int b = 0; int c = 0; int iterations = 0;
    int a_tallies = 0, b_tallies = 0, c_tallies = 0;
    for (; iterations < total; iterations++, a++, b++, c++) {
      if (a == letters.size()) { a = 0; 
                               a_tallies++;
                               
                               };
      if (b == numbers.size()) { b = 0; b_tallies++;
                               
                               };
      if (c == symbols.size()) { c = 0; c_tallies++; };
      concurrent.add(String.format("%s%s%s", letters.get(a), numbers.get(b), symbols.get(c)));
      if (a_tallies > 0) {
      concurrent.add(String.format("%s%s%s", letters.get((a) % letters.size()), numbers.get((b + b_tallies) % numbers.size()), symbols.get((c + c_tallies) % symbols.size())));
      concurrent.add(String.format("%s%s%s", letters.get((a + a_tallies) % letters.size()), numbers.get((b) % numbers.size()), symbols.get((c + c_tallies) % symbols.size())));
      
    }
      
      
      
    }
    
    for (a = 0 ; a < letters.size(); a++) {
      for (b = 0 ; b < numbers.size(); b++) {
        for (c = 0 ; c < symbols.size(); c++) {
          iterative.add(String.format("%s%s%s", letters.get(a), numbers.get(b), symbols.get(c)));
        }
      }
    }
     for (String conc : concurrent) {
      // System.out.println(conc);
    }
    for (String value : iterative) {
      assert concurrent.contains(value) : value;
    }
    assert new HashSet<String>(iterative).equals(new HashSet<String>(concurrent));
    List<List<String>> collections = new ArrayList<>();
    collections.add(letters);
    collections.add(numbers);
    collections.add(symbols);
    ConcurrentLoop cl = new ConcurrentLoop(collections, new Func() {
      @Override
      public String run(List<String> items) {
        StringBuilder sb = new StringBuilder();
        for (String item : items) {
          sb.append(item);
        }
        return sb.toString();
      }
    });
    List<String> concurrent2 = new ArrayList<>();
    for (int i = 0 ; i <= cl.size() + 1; i++) {
     concurrent2.add(cl.tick());
      
    }
    for (String str : concurrent2) {
      System.out.println(str);
    }
    System.out.println(iterative.size());
    System.out.println(cl.size());
    for (String value : iterative) {
      assert concurrent2.contains(value) : value;
    }
    assert new HashSet<String>(iterative).equals(new HashSet<String>(concurrent2));
  }
  
}
