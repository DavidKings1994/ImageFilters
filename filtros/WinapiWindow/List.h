template <class TYPE> 
class List
{
private:
	template <class T> 
	struct Nodo
	{
		T data;
		Nodo *siguiente;
	};

	Nodo<TYPE> *tail;
	Nodo<TYPE> *head;
	int numElements;

public:
	List()
	{
		numElements = 0;
		tail = NULL;
		head = NULL;
	}

	TYPE get(int id)
	{
		Nodo<TYPE>* iterator = head;
		for(int i = 0; i < id; i++)
		{
			iterator = iterator->siguiente;
		}
		return iterator->data;
	}

	TYPE getFirst()
	{
		return head->data;
	}

	TYPE getLast()
	{
		return tail->data;
	}

	void add(TYPE item)
	{
		if(head == NULL)
		{
			head = new Nodo<TYPE>();
			head->data = item;
			head->siguiente = NULL;
			tail = head;
		}
		else
		{
			Nodo<TYPE> *n = new Nodo<TYPE>(); 
			n->data = item;
			n->siguiente = NULL;
			tail->siguiente = n;
			tail = tail->siguiente;
		}
		numElements++;
	}

	void erase(int id)
	{
		if(id == numElements - 1)
		{
			Nodo<TYPE>* iterator = head;
			for(int i = 0; i < numElements - 1; i++)
			{
				iterator = iterator->siguiente;
			}
			tail = iterator;
			iterator = iterator->siguiente;
			tail->siguiente = NULL;
			delete iterator;
			numElements--;
		}
		else if(id == 0)
		{
			Nodo<TYPE>* iterator = head;
			head = head->siguiente;
			delete iterator;
			numElements--;
		}
		else if(id > 0 && id < numElements - 1)
		{
			Nodo<TYPE>* iterator = head;
			for(int i = 0; i < id; i++)
			{
				iterator = iterator->siguiente;
			}

			Nodo<TYPE>* temp = iterator->siguiente;
			iterator->siguiente = iterator->siguiente->siguiente;
			delete temp;
			numElements--;
		}
	}

	void clear()
	{
		for(int i = 0; i < numElements - 1; i++)
		{
			Nodo<TYPE>* temp = head;
			head = head->siguiente;
			delete temp;
		}
		delete head;

		head = NULL;
		tail = NULL;
		numElements = 0;
	}

	int getSize()
	{
		return numElements;
	}
};